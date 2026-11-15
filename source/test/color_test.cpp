import std;
import star.color;
import star.math_common;
import star.string;
import star.string_view;
import star.unicode;
import star.vector;
#include "gtest/gtest.h"

using namespace Star;

namespace {

template <typename T>
concept color_data_borrowable = requires(T&& color) {
  { std::forward<T>(color).data() } -> std::same_as<Vec4F const&>;
};

static_assert(color_data_borrowable<Color&>);
static_assert(color_data_borrowable<Color const&>);
static_assert(!color_data_borrowable<Color>);
static_assert(!color_data_borrowable<Color const>);

}

TEST(ColorTest, DefaultIsTransparentBlack) {
  Color const color;
  EXPECT_EQ(color.to_rgba(), Vec4B(0, 0, 0, 0));
  EXPECT_EQ(color.to_rgba_f(), Vec4F(0, 0, 0, 0));
  EXPECT_TRUE(color.is_clear());
}

TEST(ColorTest, StrictHexAcceptsShorthandAndFullMixedCase) {
  struct HexCase {
    std::string_view input;
    Vec4B bytes;
    std::string_view canonical;
  };
  std::array const cases{
    HexCase{"aBc", Vec4B(170, 187, 204, 255), "aabbcc"},
    HexCase{"AbC4", Vec4B(170, 187, 204, 68), "aabbcc44"},
    HexCase{"01aBfE", Vec4B(1, 171, 254, 255), "01abfe"},
    HexCase{"01AbFe80", Vec4B(1, 171, 254, 128), "01abfe80"},
    HexCase{"0000", Vec4B(0, 0, 0, 0), "00000000"},
    HexCase{"FfFf", Vec4B(255, 255, 255, 255), "ffffff"}
  };
  for (auto const& test : cases) {
    SCOPED_TRACE(test.input);
    auto bytes = Color::parse_rgba_hex(test.input);
    ASSERT_TRUE(bytes);
    EXPECT_EQ(*bytes, test.bytes);
    auto color = Color::from_hex(test.input);
    ASSERT_TRUE(color);
    EXPECT_EQ(color->to_rgba(), test.bytes);
    EXPECT_EQ(color->to_hex().utf8(), test.canonical);
    std::string const prefixed_input = std::string("#") + std::string(test.input);
    auto prefixed = Color::create(prefixed_input);
    ASSERT_TRUE(prefixed);
    EXPECT_EQ(prefixed->to_rgba(), test.bytes);
  }
}

TEST(ColorTest, StrictHexRejectsLengthsAndEveryInvalidNibble) {
  for (std::size_t length = 0; length <= 10; ++length) {
    if (length == 3 || length == 4 || length == 6 || length == 8)
      continue;
    SCOPED_TRACE(length);
    std::string const input(length, 'a');
    auto result = Color::parse_rgba_hex(input);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<ColorException>());
  }
  for (std::size_t length : {3u, 4u, 6u, 8u}) {
    for (std::size_t position = 0; position < length; ++position) {
      for (char invalid : {'g', 'G', '/', ':', ' ', '#', '\0', static_cast<char>(0x80), static_cast<char>(0xff)}) {
        SCOPED_TRACE(length);
        SCOPED_TRACE(position);
        SCOPED_TRACE(static_cast<unsigned char>(invalid));
        std::string input(length, 'a');
        input[position] = invalid;
        auto bytes = Color::parse_rgba_hex(input);
        ASSERT_FALSE(bytes);
        EXPECT_TRUE(bytes.error().is<ColorException>());
        auto color = Color::from_hex(input);
        ASSERT_FALSE(color);
        EXPECT_TRUE(color.error().is<ColorException>());
      }
    }
  }
  for (std::string_view input : {std::string_view("\xc3\xa9""f", 3),
      std::string_view("ab\0def", 6), std::string_view("#abc")}) {
    auto result = Color::from_hex(input);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<ColorException>());
  }
  auto recovered = Color::from_hex("1234");
  ASSERT_TRUE(recovered);
  EXPECT_EQ(recovered->to_rgba(), Vec4B(17, 34, 51, 68));
  auto prefixed = Color::create("#ABCDEF");
  ASSERT_TRUE(prefixed);
  EXPECT_EQ(prefixed->to_rgba(), Vec4B(171, 205, 239, 255));
}

TEST(ColorTest, PackedWordsUseArgbNumericOrder) {
  EXPECT_EQ(Color::from_argb32(0x7f123456u).to_rgba(), Vec4B(0x12, 0x34, 0x56, 0x7f));
  EXPECT_EQ(Color::rgba(0x12, 0x34, 0x56, 0x7f).to_argb32(), 0x7f123456u);
  EXPECT_EQ(Color::from_argb32(0xff000001u).to_rgba(), Vec4B(0, 0, 1, 255));
  EXPECT_EQ(Color::from_argb32(0x00123456u).to_rgba(), Vec4B(0x12, 0x34, 0x56, 0));
  EXPECT_EQ(Color::rgba(255, 0, 128, 255).to_argb32(), 0xffff0080u);
}

TEST(ColorTest, HsvSectorsGrayAndClampHaveIndependentExpectedBytes) {
  struct HsvCase {
    float hue;
    Vec3B rgb;
  };
  std::array const cases{
    HsvCase{0.0f, Vec3B(255, 0, 0)},
    HsvCase{1.0f / 6.0f, Vec3B(255, 255, 0)},
    HsvCase{1.0f / 3.0f, Vec3B(0, 255, 0)},
    HsvCase{0.5f, Vec3B(0, 255, 255)},
    HsvCase{2.0f / 3.0f, Vec3B(0, 0, 255)},
    HsvCase{5.0f / 6.0f, Vec3B(255, 0, 255)},
    HsvCase{1.0f, Vec3B(255, 0, 0)},
    HsvCase{0.125f, Vec3B(255, 191, 0)},
    HsvCase{0.375f, Vec3B(0, 255, 64)},
    HsvCase{0.625f, Vec3B(0, 64, 255)},
    HsvCase{0.875f, Vec3B(255, 0, 191)}
  };
  for (auto const& test : cases) {
    SCOPED_TRACE(test.hue);
    Color const color = Color::hsva(test.hue, 1.0f, 1.0f, 0.5f);
    EXPECT_EQ(color.to_rgb(), test.rgb);
    EXPECT_EQ(color.alpha(), 127);
    EXPECT_FLOAT_EQ(color.alpha_f(), 0.5f);
  }
  EXPECT_EQ(Color::hsva(0.75f, 0.0f, 0.25f, 0.5f).to_rgba(), Vec4B(64, 64, 64, 127));
  EXPECT_EQ(Color::hsv(-0.25f, 1.0f, 1.0f).to_rgb(), Vec3B(255, 0, 0));
  EXPECT_EQ(Color::hsv(1.25f, 1.0f, 1.0f).to_rgb(), Vec3B(255, 0, 0));
  Color clamped = Color::rgb(0, 255, 0);
  clamped.set_hue(-0.25f);
  EXPECT_EQ(clamped.to_rgb(), Vec3B(255, 0, 0));
  clamped = Color::rgb(0, 0, 255);
  clamped.set_hue(1.25f);
  EXPECT_EQ(clamped.to_rgb(), Vec3B(255, 0, 0));
  auto gray = Color::gray_f(0.25f).to_hsva();
  EXPECT_FLOAT_EQ(gray[0], 0.0f);
  EXPECT_FLOAT_EQ(gray[1], 0.0f);
  EXPECT_FLOAT_EQ(gray[2], 0.25f);
}

TEST(ColorTest, HueShiftsWrapNegativeAndMultipleTurns) {
  struct ShiftCase {
    float shift;
    Vec3B rgb;
  };
  std::array const cases{
    ShiftCase{0.0f, Vec3B(255, 0, 0)},
    ShiftCase{1.0f / 6.0f, Vec3B(255, 255, 0)},
    ShiftCase{1.0f / 3.0f, Vec3B(0, 255, 0)},
    ShiftCase{-1.0f / 3.0f, Vec3B(0, 0, 255)},
    ShiftCase{-1.0f / 6.0f, Vec3B(255, 0, 255)},
    ShiftCase{0.5f, Vec3B(0, 255, 255)},
    ShiftCase{-0.5f, Vec3B(0, 255, 255)},
    ShiftCase{3.5f, Vec3B(0, 255, 255)},
    ShiftCase{-2.5f, Vec3B(0, 255, 255)},
    ShiftCase{4.0f, Vec3B(255, 0, 0)},
    ShiftCase{-4.0f, Vec3B(255, 0, 0)}
  };
  for (auto const& test : cases) {
    SCOPED_TRACE(test.shift);
    Color color = Color::rgba_f(1.0f, 0.0f, 0.0f, 0.375f);
    color.hue_shift(test.shift);
    EXPECT_EQ(color.to_rgb(), test.rgb);
    EXPECT_FLOAT_EQ(color.alpha_f(), 0.375f);
    auto raw = Color::hue_shift_rgba(Vec4B(255, 0, 0, 37), test.shift);
    EXPECT_EQ(raw, Vec4B(test.rgb[0], test.rgb[1], test.rgb[2], 37));
  }
  std::array const primaries{
    Vec4B(255, 0, 0, 51), Vec4B(255, 255, 0, 51), Vec4B(0, 255, 0, 51),
    Vec4B(0, 255, 255, 51), Vec4B(0, 0, 255, 51), Vec4B(255, 0, 255, 51)
  };
  for (std::size_t sector = 0; sector < primaries.size(); ++sector) {
    for (float shift : {-2.5f, -0.5f, 0.5f, 3.5f}) {
      SCOPED_TRACE(sector);
      SCOPED_TRACE(shift);
      auto const& expected = primaries[(sector + 3) % primaries.size()];
      EXPECT_EQ(Color::hue_shift_rgba(primaries[sector], shift), expected);
      Color color = Color::rgba(primaries[sector]);
      color.hue_shift(shift);
      EXPECT_EQ(color.to_rgb(), Vec3B(expected[0], expected[1], expected[2]));
    }
  }
}

TEST(ColorTest, HueShiftRetainsNonPrimaryHalfByteBoundaries) {
  struct HueCase {
    Vec4B input;
    float shift;
    Vec4B expected;
  };
  std::array const cases{
    HueCase{Vec4B(38, 96, 255, 255), 0.25f, Vec4B(255, 38, 204, 255)},
    HueCase{Vec4B(0, 220, 233, 255), 1.0f / 12.0f, Vec4B(0, 103, 233, 255)},
    HueCase{Vec4B(0, 220, 233, 255), 0.25f, Vec4B(130, 0, 233, 255)}
  };
  for (auto const& test : cases) {
    Color color = Color::rgba(test.input);
    color.hue_shift(test.shift);
    EXPECT_EQ(color.to_rgba(), test.expected);
  }
}

TEST(ColorTest, RawHueShiftPreservesEveryAlphaByteAndZeroShiftIdentity) {
  for (unsigned alpha = 0; alpha <= 255; ++alpha) {
    SCOPED_TRACE(alpha);
    auto const byte = static_cast<std::uint8_t>(alpha);
    Vec4B const irregular(17, 93, 201, byte);
    EXPECT_EQ(Color::hue_shift_rgba(irregular, 0.0f), irregular);
    for (float shift : {-2.5f, 0.0f, 0.5f, 3.5f}) {
      EXPECT_EQ(Color::hue_shift_rgba(Vec4B(73, 73, 73, byte), shift), Vec4B(73, 73, 73, byte));
      EXPECT_EQ(Color::hue_shift_rgba(irregular, shift)[3], byte);
    }
    EXPECT_EQ(Color::hue_shift_rgba(Vec4B(255, 0, 0, byte), -2.5f), Vec4B(0, 255, 255, byte));
  }
}

TEST(ColorTest, HsvRoundTripPreservesFiniteRgbWithinOneByte) {
  for (unsigned red : {0u, 1u, 63u, 128u, 254u, 255u}) {
    for (unsigned green : {0u, 1u, 63u, 128u, 254u, 255u}) {
      for (unsigned blue : {0u, 1u, 63u, 128u, 254u, 255u}) {
        SCOPED_TRACE(red);
        SCOPED_TRACE(green);
        SCOPED_TRACE(blue);
        Color const original = Color::rgba(red, green, blue, 127);
        Color const rebuilt = Color::hsva(original.to_hsva());
        EXPECT_NEAR(rebuilt.red_f(), original.red_f(), 0.00001f);
        EXPECT_NEAR(rebuilt.green_f(), original.green_f(), 0.00001f);
        EXPECT_NEAR(rebuilt.blue_f(), original.blue_f(), 0.00001f);
        EXPECT_LE(std::abs(int(rebuilt.red()) - int(red)), 1);
        EXPECT_LE(std::abs(int(rebuilt.green()) - int(green)), 1);
        EXPECT_LE(std::abs(int(rebuilt.blue()) - int(blue)), 1);
        EXPECT_FLOAT_EQ(rebuilt.alpha_f(), original.alpha_f());
      }
    }
  }
}

TEST(ColorTest, RgbRoundsWhileAlphaAndFloatHelpersTruncate) {
  Color const half = Color::rgba_f(0.5f, 0.5f, 0.5f, 0.5f);
  EXPECT_EQ(half.to_rgba(), Vec4B(128, 128, 128, 127));
  EXPECT_EQ(float_to_byte(0.5f), 127);
  EXPECT_EQ(Color::rgb_float_to_bytes(Vec3F(0.5f, 0.5f, 0.5f)), Vec3B(127, 127, 127));
  EXPECT_EQ(Color::rgba_float_to_bytes(Vec4F(0.5f, 0.5f, 0.5f, 0.5f)), Vec4B(127, 127, 127, 127));
  for (unsigned byte : {0u, 1u, 63u, 127u, 253u, 254u}) {
    SCOPED_TRACE(byte);
    float const below = (float(byte) + 0.25f) / 255.0f;
    float const above = (float(byte) + 0.75f) / 255.0f;
    Color const low = Color::rgba_f(below, below, below, below);
    Color const high = Color::rgba_f(above, above, above, above);
    EXPECT_EQ(low.red(), byte);
    EXPECT_EQ(low.alpha(), byte);
    EXPECT_EQ(high.to_rgb(), Vec3B(byte + 1, byte + 1, byte + 1));
    EXPECT_EQ(high.alpha(), byte);
    EXPECT_EQ(float_to_byte(below), byte);
    EXPECT_EQ(float_to_byte(above), byte);
  }
  EXPECT_EQ(float_to_byte(0.0f), 0);
  EXPECT_EQ(float_to_byte(1.0f), 255);
  EXPECT_EQ(float_to_byte(-1.0f, true), 0);
  EXPECT_EQ(float_to_byte(2.0f, true), 255);
  EXPECT_EQ(Color::rgba_f(-1.0f, 2.0f, 0.0f, 2.0f).to_rgba(), Vec4B(0, 255, 0, 255));
  EXPECT_EQ(Color::rgba_float_to_bytes(Vec4F(-1.0f, 2.0f, 0.0f, 1.0f)), Vec4B(0, 255, 0, 255));
}

TEST(ColorTest, ConstGammaConversionsPreserveAlphaAndSource) {
  Color const srgb = Color::rgba_f(0.0f, 0.5f, 1.0f, 0.375f);
  Color const linear = srgb.to_linear();
  EXPECT_FLOAT_EQ(linear.red_f(), 0.0f);
  EXPECT_NEAR(linear.green_f(), 0.2140411405f, 0.000001f);
  EXPECT_NEAR(linear.blue_f(), 1.0f, 0.000001f);
  EXPECT_FLOAT_EQ(linear.alpha_f(), 0.375f);
  EXPECT_EQ(srgb.to_rgba_f(), Vec4F(0.0f, 0.5f, 1.0f, 0.375f));
  Color const restored = linear.to_srgb();
  EXPECT_NEAR(restored.green_f(), 0.5f, 0.000001f);
  EXPECT_FLOAT_EQ(restored.alpha_f(), 0.375f);
  Color const half_linear = Color::rgba_f(0.5f, 0.0f, 1.0f, 0.375f);
  EXPECT_NEAR(half_linear.to_srgb().red_f(), 0.7353569831f, 0.000001f);
  EXPECT_EQ(half_linear.to_rgba_f(), Vec4F(0.5f, 0.0f, 1.0f, 0.375f));
  EXPECT_NEAR(Color::srgb_to_linear(0.04045f), 0.00313080495f, 0.00000001f);
  EXPECT_NEAR(Color::linear_to_srgb(0.0031308f), 0.040449936f, 0.0000001f);
}

TEST(ColorTest, DataBorrowsBoundedLvalueWhileRgbaFloatOwnsItsValue) {
  Color color = Color::rgba_f(0.25f, 0.5f, 0.75f, 1.0f);
  Vec4F const& borrowed = std::as_const(color).data();
  Vec4F const owned = color.to_rgba_f();
  EXPECT_EQ(borrowed.size(), 4u);
  EXPECT_EQ(borrowed, Vec4F(0.25f, 0.5f, 0.75f, 1.0f));
  color.set_red_f(0.875f);
  EXPECT_FLOAT_EQ(borrowed[0], 0.875f);
  EXPECT_FLOAT_EQ(owned[0], 0.25f);
}

TEST(ColorTest, NamedPalettePreservesCustomValuesAndAsciiCase) {
  struct NamedCase {
    std::string_view name;
    Vec4B bytes;
  };
  std::array const cases{
    NamedCase{"red", Vec4B(255, 73, 66, 255)},
    NamedCase{"orange", Vec4B(255, 180, 47, 255)},
    NamedCase{"yellow", Vec4B(255, 239, 30, 255)},
    NamedCase{"green", Vec4B(79, 230, 70, 255)},
    NamedCase{"blue", Vec4B(38, 96, 255, 255)},
    NamedCase{"indigo", Vec4B(75, 0, 130, 255)},
    NamedCase{"violet", Vec4B(160, 119, 255, 255)},
    NamedCase{"black", Vec4B(0, 0, 0, 255)},
    NamedCase{"white", Vec4B(255, 255, 255, 255)},
    NamedCase{"magenta", Vec4B(221, 92, 249, 255)},
    NamedCase{"darkmagenta", Vec4B(142, 33, 144, 255)},
    NamedCase{"cyan", Vec4B(0, 220, 233, 255)},
    NamedCase{"darkcyan", Vec4B(0, 137, 165, 255)},
    NamedCase{"cornflowerblue", Vec4B(100, 149, 237, 255)},
    NamedCase{"gray", Vec4B(160, 160, 160, 255)},
    NamedCase{"lightgray", Vec4B(192, 192, 192, 255)},
    NamedCase{"darkgray", Vec4B(128, 128, 128, 255)},
    NamedCase{"darkgreen", Vec4B(0, 128, 0, 255)},
    NamedCase{"pink", Vec4B(255, 162, 187, 255)},
    NamedCase{"clear", Vec4B(0, 0, 0, 0)}
  };
  for (auto const& test : cases) {
    SCOPED_TRACE(test.name);
    auto lower = Color::create(test.name);
    ASSERT_TRUE(lower);
    EXPECT_EQ(lower->to_rgba(), test.bytes);
    std::string mixed(test.name);
    for (std::size_t i = 0; i < mixed.size(); i += 2)
      mixed[i] = static_cast<char>(mixed[i] - 'a' + 'A');
    auto folded = Color::create(mixed);
    ASSERT_TRUE(folded);
    EXPECT_EQ(folded->to_rgba(), test.bytes);
  }
}

TEST(ColorTest, NamedLookupValidatesFullUtf8AndDoesNotStopAtNul) {
  for (std::string_view name : {std::string_view("r\xc3\xa9""d"),
      std::string_view("red\0", 4), std::string_view("red\0blue", 8),
      std::string_view("red "), std::string_view(""), std::string_view("transparent")}) {
    auto unknown = Color::create(name);
    ASSERT_FALSE(unknown);
    EXPECT_TRUE(unknown.error().is<ColorException>());
  }
  for (std::string_view name : {std::string_view("red\xc2", 4),
      std::string_view("unknown\xff", 8), std::string_view("red\0\xc2", 5),
      std::string_view("\xc0\xaf", 2), std::string_view("red\xf4\x90\x80\x80", 7)}) {
    auto invalid = Color::create(name);
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<UnicodeException>());
    EXPECT_FALSE(invalid.error().is<ColorException>());
  }
  auto recovered = Color::create("ReD");
  ASSERT_TRUE(recovered);
  EXPECT_EQ(recovered->to_rgba(), Vec4B(255, 73, 66, 255));
}

TEST(ColorTest, NamedLookupConsumesBorrowedInputSynchronously) {
  auto parsed = [] {
    std::string storage = "prefixCoRnFlOwErBlUesuffix";
    StringView borrowed(std::string_view(storage).substr(6, 14));
    auto result = Color::create(borrowed);
    storage.assign(1024, 'x');
    return result;
  }();
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->to_rgba(), Vec4B(100, 149, 237, 255));
}

TEST(ColorTest, ColorTemperature) {
  Color temp1000 = Color::temperature(1000);
  Color temp3000 = Color::temperature(3000);
  Color temp5000 = Color::temperature(5000);
  Color temp6000 = Color::temperature(6000);
  Color temp7000 = Color::temperature(7000);
  Color temp10000 = Color::temperature(10000);
  Color temp20000 = Color::temperature(20000);

  EXPECT_EQ(temp1000.red(), 255);
  EXPECT_EQ(temp1000.green(), 68);
  EXPECT_EQ(temp1000.blue(), 0);
  EXPECT_EQ(temp3000.red(), 255);
  EXPECT_EQ(temp3000.green(), 177);
  EXPECT_EQ(temp3000.blue(), 110);
  EXPECT_EQ(temp5000.red(), 255);
  EXPECT_EQ(temp5000.green(), 228);
  EXPECT_EQ(temp5000.blue(), 206);
  EXPECT_EQ(temp6000.red(), 255);
  EXPECT_EQ(temp6000.green(), 246);
  EXPECT_EQ(temp6000.blue(), 237);
  EXPECT_EQ(temp7000.red(), 243);
  EXPECT_EQ(temp7000.green(), 242);
  EXPECT_EQ(temp7000.blue(), 255);
  EXPECT_EQ(temp10000.red(), 202);
  EXPECT_EQ(temp10000.green(), 218);
  EXPECT_EQ(temp10000.blue(), 255);
  EXPECT_EQ(temp20000.red(), 171);
  EXPECT_EQ(temp20000.green(), 198);
  EXPECT_EQ(temp20000.blue(), 255);
}

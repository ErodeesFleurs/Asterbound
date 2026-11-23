module;
import std;
import star.string;
import star.text_formatter;

import star.string_view;
import star.vector;
import star.format;

import star.math_common;
namespace Star {

struct ColorExceptionTag {
  static constexpr std::string_view name() { return "ColorException"; }
};
using ColorException = StarError<ColorExceptionTag, StarException>;

class Color {
  // Floating-point operations require finite inputs.
public:
  static Color const palette_red;
  static Color const palette_orange;
  static Color const palette_yellow;
  static Color const palette_green;
  static Color const palette_blue;
  static Color const palette_indigo;
  static Color const palette_violet;
  static Color const palette_black;
  static Color const palette_white;
  static Color const palette_magenta;
  static Color const palette_dark_magenta;
  static Color const palette_cyan;
  static Color const palette_dark_cyan;
  static Color const palette_cornflower_blue;
  static Color const palette_gray;
  static Color const palette_light_gray;
  static Color const palette_dark_gray;
  static Color const palette_dark_green;
  static Color const palette_pink;
  static Color const palette_clear;

  // Some useful conversion methods for dealing with Vec3 / Vec4 as colors
  static Vec3F rgb_bytes_to_float(Vec3B const& b);
  static Vec3B rgb_float_to_bytes(Vec3F const& f, bool do_clamp = true);
  static Vec4F rgba_bytes_to_float(Vec4B const& b);
  static Vec4B rgba_float_to_bytes(Vec4F const& f, bool do_clamp = true);

  static Color rgb_f(float r, float g, float b);
  static Color rgba_f(float r, float g, float b, float a);
  static Color rgb_f(Vec3F const& c);
  static Color rgba_f(Vec4F const& c);

  static Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b);
  static Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);
  static Color rgb(Vec3B const& c);
  static Color rgba(Vec4B const& c);

  static Color hsv(float h, float s, float b);
  static Color hsva(float h, float s, float b, float a);
  static Color hsv(Vec3F const& c);
  static Color hsva(Vec4F const& c);

  static Color gray_f(float g);
  static Color gray(std::uint8_t g);

  // Strict ASCII RGB/RGBA hex: 3, 4, 6 or 8 digits, without '#'.
  [[nodiscard]] static std::expected<Color, StarException> from_hex(StringView s);

  // Numeric AARRGGBB, independent of host byte order.
  static Color from_argb32(std::uint32_t v);

  // Color from temperature in Kelvin
  static Color temperature(float temp);

  // Finite shift in turns; preserves input alpha byte exactly.
  static Vec4B hue_shift_rgba(Vec4B color, float hue);
  [[nodiscard]] static std::expected<Vec4B, StarException> parse_rgba_hex(StringView s);
  // Transparent black.
  Color();

  [[nodiscard]] static std::expected<Color, StarException> create(StringView name);

  std::uint8_t red() const;
  std::uint8_t green() const;
  std::uint8_t blue() const;
  std::uint8_t alpha() const;

  void set_red(std::uint8_t r);
  void set_green(std::uint8_t g);
  void set_blue(std::uint8_t b);
  void set_alpha(std::uint8_t a);

  float red_f() const;
  float green_f() const;
  float blue_f() const;
  float alpha_f() const;

  void set_red_f(float r);
  void set_green_f(float g);
  void set_blue_f(float b);
  void set_alpha_f(float a);

  bool is_clear() const;

  // Numeric AARRGGBB, independent of host byte order.
  std::uint32_t to_argb32() const;

  Vec4B to_rgba() const;
  Vec3B to_rgb() const;
  Vec4F to_rgba_f() const;
  Vec3F to_rgb_f() const;

  Vec4F const& rgba_channels() const &;
  Vec4F const& rgba_channels() const && = delete;

  Vec4F to_hsva() const;

  float hue() const;
  float saturation() const;
  float value() const;

  void set_hue(float hue);
  void set_saturation(float saturation);
  void set_value(float value);

  // Shift by finite normalized turns, wrapping negative and multiple cycles.
  void hue_shift(float hue);

  // Reduce the color toward black by the given amount, from 0.0 to 1.0.
  void fade(float value);

  String to_hex() const;

  void convert_to_linear();
  void convert_to_srgb();

  Color to_linear() const;
  Color to_srgb() const;

  // Mix two colors, giving the second color the given amount
  Color mix(Color const& c, float amount = 0.5f) const;
  Color multiply(float amount) const;

  bool operator==(Color const& c) const;
  bool operator!=(Color const& c) const;
  Color operator+(Color const& c) const;
  Color operator*(Color const& c) const;
  Color& operator+=(Color const& c);
  Color& operator*=(Color const& c);

  static float srgb_to_linear(float in);
  static float linear_to_srgb(float in);
private:
  Vec4F m_data;
};

void write_text(TextBuffer& buffer, Color const& c);

inline Vec3F Color::rgb_bytes_to_float(Vec3B const& b) {
  return Vec3F(byte_to_float(b[0]), byte_to_float(b[1]), byte_to_float(b[2]));
}

inline Vec3B Color::rgb_float_to_bytes(Vec3F const& f, bool do_clamp) {
  return Vec3B(float_to_byte(f[0], do_clamp), float_to_byte(f[1], do_clamp), float_to_byte(f[2], do_clamp));
}

inline Vec4F Color::rgba_bytes_to_float(Vec4B const& b) {
  return Vec4F(byte_to_float(b[0]), byte_to_float(b[1]), byte_to_float(b[2]), byte_to_float(b[3]));
}

inline Vec4B Color::rgba_float_to_bytes(Vec4F const& f, bool do_clamp) {
  return Vec4B(float_to_byte(f[0], do_clamp), float_to_byte(f[1], do_clamp), float_to_byte(f[2], do_clamp), float_to_byte(f[3], do_clamp));
}

}

export module star.color;

export template <> struct std::formatter<Star::Color> : Star::TextFormatter {};

export namespace Star {
  using ::Star::ColorExceptionTag;
  using ::Star::ColorException;
  using ::Star::Color;
  using ::Star::write_text;
}

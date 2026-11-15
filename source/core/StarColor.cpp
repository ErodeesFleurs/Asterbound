import std;
import star.text_formatter;
import star.string;
import star.format;
import star.interpolation;
import star.string_view;
import star.vector;
import star.color;

import star.unicode;
import star.math_common;
namespace Star {

Color const Color::Red = Color::rgba(255, 73, 66, 255);
Color const Color::Orange = Color::rgba(255, 180, 47, 255);
Color const Color::Yellow = Color::rgba(255, 239, 30, 255);
Color const Color::Green = Color::rgba(79, 230, 70, 255);
Color const Color::Blue = Color::rgba(38, 96, 255, 255);
Color const Color::Indigo = Color::rgba(75, 0, 130, 255);
Color const Color::Violet = Color::rgba(160, 119, 255, 255);
Color const Color::Black = Color::rgba(0, 0, 0, 255);
Color const Color::White = Color::rgba(255, 255, 255, 255);
Color const Color::Magenta = Color::rgba(221, 92, 249, 255);
Color const Color::DarkMagenta = Color::rgba(142, 33, 144, 255);
Color const Color::Cyan = Color::rgba(0, 220, 233, 255);
Color const Color::DarkCyan = Color::rgba(0, 137, 165, 255);
Color const Color::CornFlowerBlue = Color::rgba(100, 149, 237, 255);
Color const Color::Gray = Color::rgba(160, 160, 160, 255);
Color const Color::LightGray = Color::rgba(192, 192, 192, 255);
Color const Color::DarkGray = Color::rgba(128, 128, 128, 255);
Color const Color::DarkGreen = Color::rgba(0, 128, 0, 255);
Color const Color::Pink = Color::rgba(255, 162, 187, 255);
Color const Color::Clear = Color::rgba(0, 0, 0, 0);

namespace {
constexpr std::array<std::pair<std::string_view, std::reference_wrapper<Color const>>, 20> NamedPalette{{
  {"red", std::cref(Color::Red)},
  {"orange", std::cref(Color::Orange)},
  {"yellow", std::cref(Color::Yellow)},
  {"green", std::cref(Color::Green)},
  {"blue", std::cref(Color::Blue)},
  {"indigo", std::cref(Color::Indigo)},
  {"violet", std::cref(Color::Violet)},
  {"black", std::cref(Color::Black)},
  {"white", std::cref(Color::White)},
  {"magenta", std::cref(Color::Magenta)},
  {"darkmagenta", std::cref(Color::DarkMagenta)},
  {"cyan", std::cref(Color::Cyan)},
  {"darkcyan", std::cref(Color::DarkCyan)},
  {"cornflowerblue", std::cref(Color::CornFlowerBlue)},
  {"gray", std::cref(Color::Gray)},
  {"lightgray", std::cref(Color::LightGray)},
  {"darkgray", std::cref(Color::DarkGray)},
  {"darkgreen", std::cref(Color::DarkGreen)},
  {"pink", std::cref(Color::Pink)},
  {"clear", std::cref(Color::Clear)}
}};
}

Color Color::rgb_f(const Vec3F& c) {
  return rgba_f(c[0], c[1], c[2], 1.0f);
}

Color Color::rgba_f(const Vec4F& c) {
  return rgba_f(c[0], c[1], c[2], c[3]);
}

Color Color::rgb_f(float r, float g, float b) {
  return rgba_f(r, g, b, 1.0f);
}

Color Color::rgba_f(float r, float g, float b, float a) {
  Color c;
  c.m_data[0] = std::clamp(r, 0.0f, 1.0f);
  c.m_data[1] = std::clamp(g, 0.0f, 1.0f);
  c.m_data[2] = std::clamp(b, 0.0f, 1.0f);
  c.m_data[3] = std::clamp(a, 0.0f, 1.0f);
  return c;
}

Color Color::rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  return rgba(r, g, b, 255);
}

Color Color::rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
  Color c;
  c.m_data[0] = r / 255.0f;
  c.m_data[1] = g / 255.0f;
  c.m_data[2] = b / 255.0f;
  c.m_data[3] = a / 255.0f;
  return c;
}

Color Color::from_argb32(std::uint32_t v) {
  return rgba(static_cast<std::uint8_t>((v >> 16) & 0xff),
      static_cast<std::uint8_t>((v >> 8) & 0xff),
      static_cast<std::uint8_t>(v & 0xff),
      static_cast<std::uint8_t>((v >> 24) & 0xff));
}

Color Color::temperature(float temp) {
  // Magic numbers ahoy!
  Color c;
  c.set_alpha(255);

  temp = std::clamp<float>(temp, 1000, 40000);

  temp /= 100;

  double r, g, b;
  if (temp <= 66) {
    r = 255;
    g = std::clamp<double>(99.4708025861 * std::log(temp) - 161.1195681661, 0, 255);
    if (temp <= 19) {
      b = 0;
    } else {
      b = std::clamp<double>(138.5177312231 * std::log(temp - 10) - 305.0447927307, 0, 255);
    }
  } else {
    r = std::clamp<double>(329.698727446 * std::pow(temp - 60, -0.1332047592), 0, 255);
    g = std::clamp<double>(288.1221695283 * std::pow(temp - 60, -0.0755148492), 0, 255);
    b = 255;
  }

  c.set_red_f(static_cast<float>(r) / 255.0f);
  c.set_green_f(static_cast<float>(g) / 255.0f);
  c.set_blue_f(static_cast<float>(b) / 255.0f);

  return c;
}


Color Color::rgb(Vec3B const& c) {
  return rgb(c[0], c[1], c[2]);
}

Color Color::rgba(Vec4B const& c) {
  return rgba(c[0], c[1], c[2], c[3]);
}

Color Color::hsv(float h, float s, float v) {
  return hsva(h, s, v, 1.0f);
}

Color Color::hsva(float h, float s, float v, float a) {
  h = std::clamp(h, 0.0f, 1.0f);
  s = std::clamp(s, 0.0f, 1.0f);
  v = std::clamp(v, 0.0f, 1.0f);
  a = std::clamp(a, 0.0f, 1.0f);

  Color retColor;
  if (s == 0.0f) {
    retColor.set_red_f(v);
    retColor.set_green_f(v);
    retColor.set_blue_f(v);
    retColor.set_alpha_f(a);
  } else {
    float var_h, var_i, var_1, var_2, var_3, var_r, var_g, var_b;

    var_h = h * 6.0f;
    if (var_h == 6.0f)
      var_h = 0.0f; // H must be < 1

    var_i = std::floor(var_h);

    {
#pragma float_control(precise, on)
#pragma clang fp contract(off)
      // Keep the legacy separate multiply/subtract rounding at byte boundaries.
      // Fusing these terms can move a secondary channel across a half-byte.
      var_1 = v * (1.0f - s);
      var_2 = v * (1.0f - s * (var_h - var_i));
      var_3 = v * (1.0f - s * (1.0f - (var_h - var_i)));
    }

    if (var_i == 0) {
      var_r = v;
      var_g = var_3;
      var_b = var_1;
    } else if (var_i == 1) {
      var_r = var_2;
      var_g = v;
      var_b = var_1;
    } else if (var_i == 2) {
      var_r = var_1;
      var_g = v;
      var_b = var_3;
    } else if (var_i == 3) {
      var_r = var_1;
      var_g = var_2;
      var_b = v;
    } else if (var_i == 4) {
      var_r = var_3;
      var_g = var_1;
      var_b = v;
    } else {
      var_r = v;
      var_g = var_1;
      var_b = var_2;
    }

    retColor.set_red_f(var_r);
    retColor.set_green_f(var_g);
    retColor.set_blue_f(var_b);
    retColor.set_alpha_f(a);
  }
  return retColor;
}

Color Color::hsv(Vec3F const& c) {
  return Color::hsv(c[0], c[1], c[2]);
}

Color Color::hsva(Vec4F const& c) {
  return Color::hsva(c[0], c[1], c[2], c[3]);
}

Color Color::gray_f(float g) {
  return Color::rgb_f(g, g, g);
}

Color Color::gray(std::uint8_t g) {
  return Color::rgb(g, g, g);
}

Color::Color() = default;

std::expected<Color, StarException> Color::create(StringView name) {
  auto bytes = name.utf8();
  if (bytes.starts_with('#'))
    return from_hex(bytes.substr(1));
  // Validate the full extent before lookup, including unknown names and NULs.
  auto validated = for_each_codepoint(bytes, [](Utf32Type, std::size_t, std::size_t) {});
  if (!validated)
    return std::unexpected(std::move(validated.error()));
  for (auto const& [key, color] : NamedPalette) {
    if (bytes.size() != key.size())
      continue;
    bool matches = true;
    for (std::size_t i = 0; i != key.size(); ++i) {
      auto c = bytes[i];
      if (c >= 'A' && c <= 'Z')
        c += 'a' - 'A';
      if (c != key[i]) {
        matches = false;
        break;
      }
    }
    if (matches)
      return color.get();
  }
  return std::unexpected(ColorException(Star::format("Named color {} not found", name), false));
}

float Color::red_f() const {
  return m_data[0];
}

float Color::green_f() const {
  return m_data[1];
}

float Color::blue_f() const {
  return m_data[2];
}

float Color::alpha_f() const {
  return m_data[3];
}

bool Color::is_clear() const {
  return m_data[3] == 0;
}

std::uint8_t Color::red() const {
  return std::uint8_t(std::round(m_data[0] * 255));
}

std::uint8_t Color::green() const {
  return std::uint8_t(std::round(m_data[1] * 255));
}

std::uint8_t Color::blue() const {
  return std::uint8_t(std::round(m_data[2] * 255));
}

std::uint8_t Color::alpha() const {
  return std::uint8_t(m_data[3] * 255);
}

void Color::set_red_f(float r) { m_data[0] = std::clamp(r, 0.0f, 1.0f); }

void Color::set_green_f(float g) { m_data[1] = std::clamp(g, 0.0f, 1.0f); }

void Color::set_blue_f(float b) { m_data[2] = std::clamp(b, 0.0f, 1.0f); }

void Color::set_alpha_f(float a) { m_data[3] = std::clamp(a, 0.0f, 1.0f); }

void Color::set_red(std::uint8_t r) {
  m_data[0] = r / 255.0f;
}

void Color::set_green(std::uint8_t g) {
  m_data[1] = g / 255.0f;
}

void Color::set_blue(std::uint8_t b) {
  m_data[2] = b / 255.0f;
}

void Color::set_alpha(std::uint8_t a) {
  m_data[3] = a / 255.0f;
}

std::uint32_t Color::to_argb32() const {
  return (std::uint32_t(alpha()) << 24) | (std::uint32_t(red()) << 16)
      | (std::uint32_t(green()) << 8) | std::uint32_t(blue());
}

std::expected<Color, StarException> Color::from_hex(StringView s) {
  auto bytes = parse_rgba_hex(s);
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  return Color::rgba(*bytes);
}

Vec4B Color::to_rgba() const {
  return Vec4B(red(), green(), blue(), alpha());
}

Vec3B Color::to_rgb() const {
  return Vec3B(red(), green(), blue());
}

Vec4F Color::to_rgba_f() const {
  return Vec4F(red_f(), green_f(), blue_f(), alpha_f());
}

Vec3F Color::to_rgb_f() const {
  return Vec3F(red_f(), green_f(), blue_f());
}

Vec4F const& Color::data() const & {
  return m_data;
}

Vec4F Color::to_hsva() const {
  float h, s, v;

  float var_r = red_f();
  float var_g = green_f();
  float var_b = blue_f();

  // Min. value of RGB
  float var_min = std::min(std::min(var_r, var_g), var_b);

  // Max. value of RGB
  float var_max = std::max(std::max(var_r, var_g), var_b);

  // Delta RGB value
  float del_max = var_max - var_min;

  v = var_max;

  if (del_max == 0.0f) { // This is a gray, no chroma...
    h = 0.0f;
    s = 0.0f;
  } else { // Chromatic data
    s = del_max / var_max;

    float del_r = (((var_max - var_r) / 6.0f) + (del_max / 2.0f)) / del_max;
    float del_g = (((var_max - var_g) / 6.0f) + (del_max / 2.0f)) / del_max;
    float del_b = (((var_max - var_b) / 6.0f) + (del_max / 2.0f)) / del_max;

    if (var_r == var_max)
      h = del_b - del_g;
    else if (var_g == var_max)
      h = (1.0f / 3.0f) + del_r - del_b;
    else
      /*if (var_b == var_max)*/ h = (2.0f / 3.0f) + del_g - del_r;

    if (h < 0.0f)
      h += 1.0f;
    if (h >= 1.0f)
      h -= 1.0f;
  }

  return Vec4F(h, s, v, alpha_f());
}

float Color::hue() const {
  return to_hsva()[0];
}

float Color::saturation() const {
  // Min. value of RGB
  float var_min = std::min(std::min(m_data[0], m_data[1]), m_data[2]);

  // Max. value of RGB
  float var_max = std::max(std::max(m_data[0], m_data[1]), m_data[2]);

  // Delta RGB value
  float del_max = var_max - var_min;

  if (del_max == 0.0f) { // This is a gray, no chroma...
    return 0.0f;
  } else
    return del_max / var_max;
}

float Color::value() const {
  return std::max(std::max(m_data[0], m_data[1]), m_data[2]);
}

void Color::set_hue(float h) {
  // Scalar S/V getters preserve fast-math rounding; sharing a full HSV tuple does not.
  *this = Color::hsva(std::clamp(h, 0.0f, 1.0f), saturation(), value(), alpha_f());
}

void Color::set_saturation(float s) {
  auto hsva = to_hsva();
  *this = Color::hsva(hsva[0], std::clamp(s, 0.0f, 1.0f), hsva[2], alpha_f());
}

void Color::set_value(float v) {
  auto hsva = to_hsva();
  *this = Color::hsva(hsva[0], hsva[1], std::clamp(v, 0.0f, 1.0f), alpha_f());
}

void Color::hue_shift(float h) {
  set_hue(positive_mod(hue() + h, 1.0f));
}

void Color::fade(float value) {
  m_data *= (1.0f - value);
  m_data.clamp(0.0f, 1.0f);
}

bool Color::operator==(const Color& c) const {
  return m_data == c.m_data;
}

bool Color::operator!=(const Color& c) const {
  return m_data != c.m_data;
}

void write_text(TextBuffer& buffer, Color const& c) {
  append_text(buffer, c.to_rgba_f());
}

float Color::srgb_to_linear(float in) {
  const float a = 0.055f;
  if (in <= 0.04045f)
    return in / 12.92f;
  return std::powf((in + a) / (1.0f + a), 2.4f);
}

float Color::linear_to_srgb(float in) {
  const float a = 0.055f;
  if (in <= 0.0031308f)
    return 12.92f * in;
  return (1.0f + a) * std::powf(in, 1.0f / 2.4f) - a;
}

String Color::to_hex() const {
  constexpr std::string_view digits = "0123456789abcdef";
  auto rgba = to_rgba();
  auto count = rgba[3] == 255 ? std::size_t(3) : std::size_t(4);
  std::array<char, 8> output{};
  for (std::size_t i = 0; i != count; ++i) {
    output[2 * i] = digits[rgba[i] >> 4];
    output[2 * i + 1] = digits[rgba[i] & 0xf];
  }
  return String(std::string_view(output.data(), count * 2));
}

void Color::convert_to_linear() {
  set_red_f(srgb_to_linear(red_f()));
  set_green_f(srgb_to_linear(green_f()));
  set_blue_f(srgb_to_linear(blue_f()));
}

void Color::convert_to_srgb() {
  set_red_f(linear_to_srgb(red_f()));
  set_green_f(linear_to_srgb(green_f()));
  set_blue_f(linear_to_srgb(blue_f()));
}

Color Color::to_linear() const {
  Color c = *this;
  c.convert_to_linear();
  return c;
}

Color Color::to_srgb() const {
  Color c = *this;
  c.convert_to_srgb();
  return c;
}

Color Color::mix(Color const& c, float amount) const {
  return Color::rgba_f(lerp(std::clamp(amount, 0.0f, 1.0f), to_rgba_f(), c.to_rgba_f()));
}

Color Color::multiply(float amount) const {
  return Color::rgba_f(m_data * amount);
}

Color Color::operator+(Color const& c) const {
  return Color::rgba_f(m_data + c.to_rgba_f());
}

Color Color::operator*(Color const& c) const { return Color::rgba_f(m_data.piecewise_multiply(c.to_rgba_f())); }

Color& Color::operator+=(Color const& c) {
  return * this = *this + c;
}

Color& Color::operator*=(Color const& c) {
  return * this = *this * c;
}

Vec4B Color::hue_shift_rgba(Vec4B color, float hue) {
  float h, s, v;

  float var_r = color[0] / 255.0f;
  float var_g = color[1] / 255.0f;
  float var_b = color[2] / 255.0f;

  // Min. value of RGB
  float var_min = std::min(std::min(var_r, var_g), var_b);

  // Max. value of RGB
  float var_max = std::max(std::max(var_r, var_g), var_b);

  // Delta RGB value
  float del_max = var_max - var_min;

  v = var_max;

  if (del_max == 0.0f) { // This is a gray, no chroma...
    h = 0.0f;
    s = 0.0f;
  } else { // Chromatic data
    s = del_max / var_max;

    float vd = 1.0f / 6.0f;
    float dmh = del_max * 0.5f;
    float dmi = 1.0f / del_max;
    float del_r = (((var_max - var_r) * vd) + dmh) * dmi;
    float del_g = (((var_max - var_g) * vd) + dmh) * dmi;
    float del_b = (((var_max - var_b) * vd) + dmh) * dmi;

    if (var_r == var_max)
      h = del_b - del_g;
    else if (var_g == var_max)
      h = (1.0f / 3.0f) + del_r - del_b;
    else
      h = (2.0f / 3.0f) + del_g - del_r;

    if (h < 0.0f)
      h += 1.0f;
    if (h >= 1.0f)
      h -= 1.0f;
  }

  // Preserve the original add/subtract arithmetic for ordinary normalized shifts.
  // Normalize larger offsets before adding so every finite shift remains bounded.
  if (hue < 0.0f || hue >= 1.0f)
    hue = positive_mod(hue, 1.0f);
  h += hue;
  if (h >= 1.0f)
    h -= 1.0f;

  if (s == 0.0f) {
    auto c = std::uint8_t(std::round(v * 255));
    return Vec4B(c, c, c, color[3]);
  } else {
    float var_h, var_i, var_1, var_2, var_3, var_r, var_g, var_b;

    var_h = h * 6.0f;
    if (var_h == 6.0f)
      var_h = 0.0f; // H must be < 1

    var_i = std::floor(var_h);

    var_1 = v * (1.0f - s);
    var_2 = v * (1.0f - s * (var_h - var_i));
    var_3 = v * (1.0f - s * (1.0f - (var_h - var_i)));

    if (var_i == 0) {
      var_r = v;
      var_g = var_3;
      var_b = var_1;
    } else if (var_i == 1) {
      var_r = var_2;
      var_g = v;
      var_b = var_1;
    } else if (var_i == 2) {
      var_r = var_1;
      var_g = v;
      var_b = var_3;
    } else if (var_i == 3) {
      var_r = var_1;
      var_g = var_2;
      var_b = v;
    } else if (var_i == 4) {
      var_r = var_3;
      var_g = var_1;
      var_b = v;
    } else {
      var_r = v;
      var_g = var_1;
      var_b = var_2;
    }

    return Vec4B(std::uint8_t(std::round(var_r * 255)), std::uint8_t(std::round(var_g * 255)), std::uint8_t(std::round(var_b * 255)), color[3]);
  }
}

std::expected<Vec4B, StarException> Color::parse_rgba_hex(StringView s) {
  auto bytes = s.utf8();
  auto size = bytes.size();
  if (size != 3 && size != 4 && size != 6 && size != 8)
    return std::unexpected(ColorException(Star::format("Improper size {} for hex string '{}' in Color::parse_rgba_hex", size, s), false));

  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  Vec4B result(0, 0, 0, 255);
  bool short_form = size <= 4;
  auto count = short_form ? size : size / 2;
  for (std::size_t i = 0; i != count; ++i) {
    int high = nibble(bytes[short_form ? i : 2 * i]);
    int low = short_form ? high : nibble(bytes[2 * i + 1]);
    if (high < 0 || low < 0)
      return std::unexpected(ColorException(Star::format("Invalid hex digit in color '{}'", s), false));
    result[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return result;
}

}

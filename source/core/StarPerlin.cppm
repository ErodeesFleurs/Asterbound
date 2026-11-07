module;
import std;
import star.format;
import star.json;
import star.string;
import star.string_view;

import star.bi_map;
import star.interpolation;
import star.random;

import star.math_common;
namespace Star {

struct PerlinExceptionTag {
  static constexpr std::string_view name() { return "PerlinException"; }
};
using PerlinException = StarError<PerlinExceptionTag, StarException>;

enum class PerlinType {
  Uninitialized,
  Perlin,
  Billow,
  RidgedMulti
};
inline constexpr auto PerlinTypeNames = sortedEnumNames(std::array{
  std::pair{PerlinType::Uninitialized, std::string_view("uninitialized")},
  std::pair{PerlinType::Perlin, std::string_view("perlin")},
  std::pair{PerlinType::Billow, std::string_view("billow")},
  std::pair{PerlinType::RidgedMulti, std::string_view("ridgedMulti")},
});
static_assert(validEnumNames<PerlinTypeNames>());

inline constexpr int PerlinSampleSize = 512;

template <typename Float>
class Perlin {
public:
  // Default constructed perlin noise is uninitialized and cannot be queried.
  Perlin();

  Perlin(unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, std::uint64_t seed);
  Perlin(PerlinType type, unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, std::uint64_t seed);
  [[nodiscard]] static std::expected<Perlin, StarException> create(Json const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<Perlin, StarException> create(Json const& json);

  Perlin(Perlin const& perlin);
  Perlin(Perlin&& perlin);

  Perlin& operator=(Perlin const& perlin);
  Perlin& operator=(Perlin&& perlin);

  [[nodiscard]] std::expected<Float, StarException> get(Float x) const;
  [[nodiscard]] std::expected<Float, StarException> get(Float x, Float y) const;
  [[nodiscard]] std::expected<Float, StarException> get(Float x, Float y, Float z) const;

  PerlinType type() const;

  unsigned octaves() const;
  Float frequency() const;
  Float amplitude() const;
  Float bias() const;
  Float alpha() const;
  Float beta() const;

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

private:
  static Float s_curve(Float t);
  static void setup(Float v, int& b0, int& b1, Float& r0, Float& r1);

  static Float at2(std::span<Float const, 2> q, Float rx, Float ry);
  static Float at3(std::span<Float const, 3> q, Float rx, Float ry, Float rz);

  Float noise1(Float arg) const;
  Float noise2(std::span<Float const, 2> vec) const;
  Float noise3(std::span<Float const, 3> vec) const;

  void normalize2(std::span<Float, 2> v) const;
  void normalize3(std::span<Float, 3> v) const;

  void init(std::uint64_t seed);

  Float perlin(Float x) const;
  Float perlin(Float x, Float y) const;
  Float perlin(Float x, Float y, Float z) const;

  Float ridgedMulti(Float x) const;
  Float ridgedMulti(Float x, Float y) const;
  Float ridgedMulti(Float x, Float y, Float z) const;

  Float billow(Float x) const;
  Float billow(Float x, Float y) const;
  Float billow(Float x, Float y, Float z) const;

  PerlinType m_type;
  std::uint64_t m_seed;

  int m_octaves;
  Float m_frequency;
  Float m_amplitude;
  Float m_bias;
  Float m_alpha;
  Float m_beta;

  // Only used for RidgedMulti
  Float m_offset;
  Float m_gain;

  std::unique_ptr<int[]> p;
  std::unique_ptr<Float[][3]> g3;
  std::unique_ptr<Float[][2]> g2;
  std::unique_ptr<Float[]> g1;
};

using PerlinF = Perlin<float>;
using PerlinD = Perlin<double>;

template <typename Float>
Float Perlin<Float>::s_curve(Float t) {
  return t * t * (3.0 - 2.0 * t);
}

template <typename Float>
void Perlin<Float>::setup(Float v, int& b0, int& b1, Float& r0, Float& r1) {
  int iv = std::floor(v);
  Float fv = v - iv;

  b0 = iv & (PerlinSampleSize - 1);
  b1 = (iv + 1) & (PerlinSampleSize - 1);
  r0 = fv;
  r1 = fv - 1.0;
}

template <typename Float>
Float Perlin<Float>::at2(std::span<Float const, 2> q, Float rx, Float ry) {
  return rx * q[0] + ry * q[1];
}

template <typename Float>
Float Perlin<Float>::at3(std::span<Float const, 3> q, Float rx, Float ry, Float rz) {
  return rx * q[0] + ry * q[1] + rz * q[2];
}

template <typename Float>
Perlin<Float>::Perlin() {
  m_type = PerlinType::Uninitialized;
  m_alpha = 0;
  m_amplitude = 0;
  m_frequency = 0;
  m_seed = 0;
  m_gain = 0;
  m_beta = 0;
  m_offset = 0;
  m_bias = 0;
  m_octaves = 0;
}

template <typename Float>
Perlin<Float>::Perlin(unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, std::uint64_t seed) {
  m_type = PerlinType::Perlin;
  m_seed = seed;

  m_octaves = octaves;
  m_frequency = freq;
  m_amplitude = amp;
  m_bias = bias;
  m_alpha = alpha;
  m_beta = beta;

  // TODO: These ought to be configurable
  m_offset = 1.0;
  m_gain = 2.0;

  init(m_seed);
}

template <typename Float>
Perlin<Float>::Perlin(PerlinType type, unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, std::uint64_t seed) {
  m_type = type;
  m_seed = seed;

  m_octaves = octaves;
  m_frequency = freq;
  m_amplitude = amp;
  m_bias = bias;
  m_alpha = alpha;
  m_beta = beta;

  // TODO: These ought to be configurable
  m_offset = 1.0;
  m_gain = 2.0;

  init(m_seed);
}

template <typename Float>
std::expected<Perlin<Float>, StarException> Perlin<Float>::create(Json const& config, std::uint64_t seed) {
  auto seeded = config.set("seed", seed);
  if (!seeded)
    return std::unexpected(std::move(seeded.error()));
  return create(*seeded);
}

template <typename Float>
std::expected<Perlin<Float>, StarException> Perlin<Float>::create(Json const& json) {
  auto seed = json.getUInt("seed");
  if (!seed) return std::unexpected(std::move(seed.error()));
  auto octaves = json.getInt("octaves", 1);
  if (!octaves) return std::unexpected(std::move(octaves.error()));
  auto frequency = json.getDouble("frequency", 1.0);
  if (!frequency) return std::unexpected(std::move(frequency.error()));
  auto amplitude = json.getDouble("amplitude", 1.0);
  if (!amplitude) return std::unexpected(std::move(amplitude.error()));
  auto bias = json.getDouble("bias", 0.0);
  if (!bias) return std::unexpected(std::move(bias.error()));
  auto alpha = json.getDouble("alpha", 2.0);
  if (!alpha) return std::unexpected(std::move(alpha.error()));
  auto beta = json.getDouble("beta", 2.0);
  if (!beta) return std::unexpected(std::move(beta.error()));
  auto offset = json.getDouble("offset", 1.0);
  if (!offset) return std::unexpected(std::move(offset.error()));
  auto gain = json.getDouble("gain", 2.0);
  if (!gain) return std::unexpected(std::move(gain.error()));
  auto typeName = json.getString("type");
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  auto type = enumValue<PerlinTypeNames>(StringView(*typeName));
  if (!type) return std::unexpected(std::move(type.error()));
  Perlin perlin;
  perlin.m_seed = *seed;
  perlin.m_octaves = *octaves;
  perlin.m_frequency = *frequency;
  perlin.m_amplitude = *amplitude;
  perlin.m_bias = *bias;
  perlin.m_alpha = *alpha;
  perlin.m_beta = *beta;
  perlin.m_offset = *offset;
  perlin.m_gain = *gain;
  perlin.m_type = *type;
  perlin.init(perlin.m_seed);
  return perlin;
}

template <typename Float>
Perlin<Float>::Perlin(Perlin const& perlin) {
  *this = perlin;
}

template <typename Float>
Perlin<Float>::Perlin(Perlin&& perlin) {
  *this = std::move(perlin);
}

template <typename Float>
Perlin<Float>& Perlin<Float>::operator=(Perlin const& perlin) {
  if (perlin.m_type == PerlinType::Uninitialized) {
    m_type = PerlinType::Uninitialized;
    p.reset();
    g3.reset();
    g2.reset();
    g1.reset();

  } else if (this != &perlin) {
    m_type = perlin.m_type;
    m_seed = perlin.m_seed;
    m_octaves = perlin.m_octaves;
    m_frequency = perlin.m_frequency;
    m_amplitude = perlin.m_amplitude;
    m_bias = perlin.m_bias;
    m_alpha = perlin.m_alpha;
    m_beta = perlin.m_beta;
    m_offset = perlin.m_offset;
    m_gain = perlin.m_gain;

    p.reset(new int[PerlinSampleSize + PerlinSampleSize + 2]);
    g3.reset(new Float[PerlinSampleSize + PerlinSampleSize + 2][3]);
    g2.reset(new Float[PerlinSampleSize + PerlinSampleSize + 2][2]);
    g1.reset(new Float[PerlinSampleSize + PerlinSampleSize + 2]);

    std::memcpy(p.get(), perlin.p.get(), (PerlinSampleSize + PerlinSampleSize + 2) * sizeof(int));
    std::memcpy(g3.get(), perlin.g3.get(), (PerlinSampleSize + PerlinSampleSize + 2) * sizeof(Float) * 3);
    std::memcpy(g2.get(), perlin.g2.get(), (PerlinSampleSize + PerlinSampleSize + 2) * sizeof(Float) * 2);
    std::memcpy(g1.get(), perlin.g1.get(), (PerlinSampleSize + PerlinSampleSize + 2) * sizeof(Float));
  }

  return *this;
}

template <typename Float>
Perlin<Float>& Perlin<Float>::operator=(Perlin&& perlin) {
  m_type = perlin.m_type;
  m_seed = perlin.m_seed;
  m_octaves = perlin.m_octaves;
  m_frequency = perlin.m_frequency;
  m_amplitude = perlin.m_amplitude;
  m_bias = perlin.m_bias;
  m_alpha = perlin.m_alpha;
  m_beta = perlin.m_beta;
  m_offset = perlin.m_offset;
  m_gain = perlin.m_gain;

  p = std::move(perlin.p);
  g3 = std::move(perlin.g3);
  g2 = std::move(perlin.g2);
  g1 = std::move(perlin.g1);

  return *this;
}

template <typename Float>
std::expected<Float, StarException> Perlin<Float>::get(Float x) const {
  switch (m_type) {
    case PerlinType::Perlin:
      return perlin(x);
    case PerlinType::Billow:
      return billow(x);
    case PerlinType::RidgedMulti:
      return ridgedMulti(x);
    default:
      return std::unexpected(PerlinException("::get called on uninitialized Perlin"));
  }
}

template <typename Float>
std::expected<Float, StarException> Perlin<Float>::get(Float x, Float y) const {
  switch (m_type) {
    case PerlinType::Perlin:
      return perlin(x, y);
    case PerlinType::Billow:
      return billow(x, y);
    case PerlinType::RidgedMulti:
      return ridgedMulti(x, y);
    default:
      return std::unexpected(PerlinException("::get called on uninitialized Perlin"));
  }
}

template <typename Float>
std::expected<Float, StarException> Perlin<Float>::get(Float x, Float y, Float z) const {
  switch (m_type) {
    case PerlinType::Perlin:
      return perlin(x, y, z);
    case PerlinType::Billow:
      return billow(x, y, z);
    case PerlinType::RidgedMulti:
      return ridgedMulti(x, y, z);
    default:
      return std::unexpected(PerlinException("::get called on uninitialized Perlin"));
  }
}

template <typename Float>
PerlinType Perlin<Float>::type() const {
  return m_type;
}

template <typename Float>
unsigned Perlin<Float>::octaves() const {
  return m_octaves;
}

template <typename Float>
Float Perlin<Float>::frequency() const {
  return m_frequency;
}

template <typename Float>
Float Perlin<Float>::amplitude() const {
  return m_amplitude;
}

template <typename Float>
Float Perlin<Float>::bias() const {
  return m_bias;
}

template <typename Float>
Float Perlin<Float>::alpha() const {
  return m_alpha;
}

template <typename Float>
Float Perlin<Float>::beta() const {
  return m_beta;
}

template <typename Float>
std::expected<Json, StarException> Perlin<Float>::toJson() const {
  auto type = enumName<PerlinTypeNames>(m_type);
  if (!type) return std::unexpected(std::move(type.error()));
  return JsonObject{
    {"seed", m_seed},
    {"octaves", m_octaves},
    {"frequency", m_frequency},
    {"amplitude", m_amplitude},
    {"bias", m_bias},
    {"alpha", m_alpha},
    {"beta", m_beta},
    {"offset", m_offset},
    {"gain", m_gain},
    {"type", String(*type)}
  };
}

template <typename Float>
inline Float Perlin<Float>::noise1(Float arg) const {
  int bx0, bx1;
  Float rx0, rx1, sx, u, v;

  setup(arg, bx0, bx1, rx0, rx1);

  sx = s_curve(rx0);
  u = rx0 * g1[p[bx0]];
  v = rx1 * g1[p[bx1]];

  return (lerp(sx, u, v));
}

template <typename Float>
inline Float Perlin<Float>::noise2(std::span<Float const, 2> vec) const {
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  Float rx0, rx1, ry0, ry1, sx, sy, a, b, u, v;
  int i, j;

  setup(vec[0], bx0, bx1, rx0, rx1);
  setup(vec[1], by0, by1, ry0, ry1);

  i = p[bx0];
  j = p[bx1];

  b00 = p[i + by0];
  b10 = p[j + by0];
  b01 = p[i + by1];
  b11 = p[j + by1];

  sx = s_curve(rx0);
  sy = s_curve(ry0);

  u = at2(g2[b00], rx0, ry0);
  v = at2(g2[b10], rx1, ry0);
  a = lerp(sx, u, v);

  u = at2(g2[b01], rx0, ry1);
  v = at2(g2[b11], rx1, ry1);
  b = lerp(sx, u, v);

  return lerp(sy, a, b);
}

template <typename Float>
inline Float Perlin<Float>::noise3(std::span<Float const, 3> vec) const {
  int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
  Float rx0, rx1, ry0, ry1, rz0, rz1, sx, sy, sz, a, b, c, d, u, v;
  int i, j;

  setup(vec[0], bx0, bx1, rx0, rx1);
  setup(vec[1], by0, by1, ry0, ry1);
  setup(vec[2], bz0, bz1, rz0, rz1);

  i = p[bx0];
  j = p[bx1];

  b00 = p[i + by0];
  b10 = p[j + by0];
  b01 = p[i + by1];
  b11 = p[j + by1];

  sx = s_curve(rx0);
  sy = s_curve(ry0);
  sz = s_curve(rz0);

  u = at3(g3[b00 + bz0], rx0, ry0, rz0);
  v = at3(g3[b10 + bz0], rx1, ry0, rz0);
  a = lerp(sx, u, v);

  u = at3(g3[b01 + bz0], rx0, ry1, rz0);
  v = at3(g3[b11 + bz0], rx1, ry1, rz0);
  b = lerp(sx, u, v);

  c = lerp(sy, a, b);

  u = at3(g3[b00 + bz1], rx0, ry0, rz1);
  v = at3(g3[b10 + bz1], rx1, ry0, rz1);
  a = lerp(sx, u, v);

  u = at3(g3[b01 + bz1], rx0, ry1, rz1);
  v = at3(g3[b11 + bz1], rx1, ry1, rz1);
  b = lerp(sx, u, v);

  d = lerp(sy, a, b);

  return lerp(sz, c, d);
}

template <typename Float>
void Perlin<Float>::normalize2(std::span<Float, 2> v) const {
  Float s;

  s = std::sqrt(v[0] * v[0] + v[1] * v[1]);
  if (s == 0.0f) {
    v[0] = 1.0f;
    v[1] = 0.0f;
  } else {
    v[0] = v[0] / s;
    v[1] = v[1] / s;
  }
}

template <typename Float>
void Perlin<Float>::normalize3(std::span<Float, 3> v) const {
  Float s;

  s = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (s == 0.0f) {
    v[0] = 1.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
  } else {
    v[0] = v[0] / s;
    v[1] = v[1] / s;
    v[2] = v[2] / s;
  }
}

template <typename Float>
void Perlin<Float>::init(std::uint64_t seed) {
  RandomSource randomSource(seed);

  p.reset(new int[PerlinSampleSize + PerlinSampleSize + 2]);
  g3.reset(new Float[PerlinSampleSize + PerlinSampleSize + 2][3]);
  g2.reset(new Float[PerlinSampleSize + PerlinSampleSize + 2][2]);
  g1.reset(new Float[PerlinSampleSize + PerlinSampleSize + 2]);

  int i, j, k;

  for (i = 0; i < PerlinSampleSize; i++) {
    p[i] = i;
    g1[i] = (Float)(static_cast<std::int64_t>(randomSource.randUInt(PerlinSampleSize * 2)) - PerlinSampleSize) / PerlinSampleSize;

    for (j = 0; j < 2; j++)
      g2[i][j] = (Float)(static_cast<std::int64_t>(randomSource.randUInt(PerlinSampleSize * 2)) - PerlinSampleSize) / PerlinSampleSize;
    normalize2(g2[i]);

    for (j = 0; j < 3; j++)
      g3[i][j] = (Float)(static_cast<std::int64_t>(randomSource.randUInt(PerlinSampleSize * 2)) - PerlinSampleSize) / PerlinSampleSize;
    normalize3(g3[i]);
  }

  while (--i) {
    k = p[i];
    p[i] = p[j = randomSource.randUInt(PerlinSampleSize - 1)];
    p[j] = k;
  }

  for (i = 0; i < PerlinSampleSize + 2; i++) {
    p[PerlinSampleSize + i] = p[i];
    g1[PerlinSampleSize + i] = g1[i];
    for (j = 0; j < 2; j++)
      g2[PerlinSampleSize + i][j] = g2[i][j];
    for (j = 0; j < 3; j++)
      g3[PerlinSampleSize + i][j] = g3[i][j];
  }
}

template <typename Float>
inline Float Perlin<Float>::perlin(Float x) const {
  int i;
  Float val, sum = 0;
  Float p, scale = 1;

  p = x * m_frequency;
  for (i = 0; i < m_octaves; i++) {
    val = noise1(p);
    sum += val / scale;
    scale *= m_alpha;
    p *= m_beta;
  }
  return sum * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::perlin(Float x, Float y) const {
  int i;
  Float val, sum = 0;
  Float p[2], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  for (i = 0; i < m_octaves; i++) {
    val = noise2(p);
    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
  }
  return sum * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::perlin(Float x, Float y, Float z) const {
  int i;
  Float val, sum = 0;
  Float p[3], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  p[2] = z * m_frequency;
  for (i = 0; i < m_octaves; i++) {
    val = noise3(p);
    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
    p[2] *= m_beta;
  }

  return sum * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::ridgedMulti(Float x) const {
  Float val, sum = 0;
  Float scale = 1;
  Float weight = 1.0;

  x *= m_frequency;
  for (int i = 0; i < m_octaves; ++i) {
    val = noise1(x);

    val = m_offset - std::fabs(val);
    val *= val;
    val *= weight;

    weight = std::clamp<Float>(val * m_gain, 0.0, 1.0);

    sum += val / scale;
    scale *= m_alpha;
    x *= m_beta;
  }

  return ((sum * 1.25) - 1.0) * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::ridgedMulti(Float x, Float y) const {
  Float val, sum = 0;
  Float p[2], scale = 1;
  Float weight = 1.0;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  for (int i = 0; i < m_octaves; ++i) {
    val = noise2(p);

    val = m_offset - std::fabs(val);
    val *= val;
    val *= weight;

    weight = std::clamp<Float>(val * m_gain, 0.0, 1.0);

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
  }

  return ((sum * 1.25) - 1.0) * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::ridgedMulti(Float x, Float y, Float z) const {
  Float val, sum = 0;
  Float p[3], scale = 1;
  Float weight = 1.0;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  p[2] = z * m_frequency;
  for (int i = 0; i < m_octaves; ++i) {
    val = noise3(p);

    val = m_offset - std::fabs(val);
    val *= val;
    val *= weight;

    weight = std::clamp<Float>(val * m_gain, 0.0, 1.0);

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
    p[2] *= m_beta;
  }

  return ((sum * 1.25) - 1.0) * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::billow(Float x) const {
  Float val, sum = 0;
  Float p, scale = 1;

  p = x * m_frequency;
  for (int i = 0; i < m_octaves; i++) {
    val = noise1(p);
    val = 2.0 * std::fabs(val) - 1.0;

    sum += val / scale;
    scale *= m_alpha;
    p *= m_beta;
  }
  return (sum + 0.5) * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::billow(Float x, Float y) const {
  Float val, sum = 0;
  Float p[2], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  for (int i = 0; i < m_octaves; i++) {
    val = noise2(p);
    val = 2.0 * std::fabs(val) - 1.0;

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
  }
  return (sum + 0.5) * m_amplitude + m_bias;
}

template <typename Float>
inline Float Perlin<Float>::billow(Float x, Float y, Float z) const {
  Float val, sum = 0;
  Float p[3], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  p[2] = z * m_frequency;
  for (int i = 0; i < m_octaves; i++) {
    val = noise3(p);
    val = 2.0 * std::fabs(val) - 1.0;

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
    p[2] *= m_beta;
  }

  return (sum + 0.5) * m_amplitude + m_bias;
}

}

export module star.perlin;

export namespace Star {
  using ::Star::PerlinExceptionTag;
  using ::Star::PerlinException;
  using ::Star::PerlinType;
  using ::Star::PerlinTypeNames;
  using ::Star::PerlinSampleSize;
  using ::Star::Perlin;
  using ::Star::PerlinF;
  using ::Star::PerlinD;
}

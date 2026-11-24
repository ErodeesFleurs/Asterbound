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
inline constexpr auto perlin_type_names = sorted_enum_names(std::array{
  std::pair{PerlinType::Uninitialized, std::string_view("uninitialized")},
  std::pair{PerlinType::Perlin, std::string_view("perlin")},
  std::pair{PerlinType::Billow, std::string_view("billow")},
  std::pair{PerlinType::RidgedMulti, std::string_view("ridgedMulti")},
});
static_assert(valid_enum_names<perlin_type_names>());

template <finite_classifiable Float>
class Perlin {
public:
  static constexpr int sample_size = 512;

  // Parsed independently of the seed so immutable configurations can be reused.
  class Parameters {
    friend class Perlin;
  public:
    Parameters() = default;
  private:
    PerlinType type = PerlinType::Uninitialized;
    unsigned octaves = 0;
    Float frequency = 0;
    Float amplitude = 0;
    Float bias = 0;
    Float alpha = 0;
    Float beta = 0;
    Float offset = 0;
    Float gain = 0;
  };

  Perlin() = default;
  Perlin(Parameters const& parameters, std::uint64_t seed);
  [[nodiscard]] static std::expected<Parameters, StarException> parse_parameters(Json const& config);
  [[nodiscard]] static std::expected<Perlin, StarException> create(PerlinType type, double const& octaves,
      double const& frequency, double const& amplitude, double const& bias, double const& alpha, double const& beta,
      std::uint64_t seed);
  [[nodiscard]] static std::expected<Perlin, StarException> create(Json const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<Perlin, StarException> create(Json const& config);

  [[nodiscard]] static std::expected<Parameters, StarException> make_parameters(PerlinType type, double const& octaves,
      double const& frequency, double const& amplitude, double const& bias, double const& alpha, double const& beta,
      double const& offset = 1, double const& gain = 2);

  Perlin(Perlin const& other);
  Perlin(Perlin&& other) noexcept;
  Perlin& operator=(Perlin const& other);
  Perlin& operator=(Perlin&& other) noexcept;
  [[nodiscard]] std::expected<Float, StarException> get(Float const& x) const;
  [[nodiscard]] std::expected<Float, StarException> get(Float const& x, Float const& y) const;
  [[nodiscard]] std::expected<Float, StarException> get(Float const& x, Float const& y, Float const& z) const;

  PerlinType type() const noexcept { return m_parameters.type; }
  unsigned octaves() const noexcept { return m_parameters.octaves; }
  Float frequency() const noexcept { return m_parameters.frequency; }
  Float amplitude() const noexcept { return m_parameters.amplitude; }
  Float bias() const noexcept { return m_parameters.bias; }
  Float alpha() const noexcept { return m_parameters.alpha; }
  Float beta() const noexcept { return m_parameters.beta; }

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

private:
  struct LatticeAxis {
    int lo, hi;
    Float near_offset, far_offset;
  };

  static Float s_curve(Float t);
  static LatticeAxis lattice_axis(Float const& coordinate);
  Float noise(std::array<LatticeAxis, 1> const& axes) const;
  Float noise(std::array<LatticeAxis, 2> const& axes) const;
  Float noise(std::array<LatticeAxis, 3> const& axes) const;

  // Only permutation and 3D gradients are indexed by the sum of two lattice indices.
  struct Tables {
    std::array<int, 2 * sample_size> p;
    std::array<Float, sample_size> g1;
    std::array<std::array<Float, 2>, sample_size> g2;
    std::array<std::array<Float, 3>, 2 * sample_size> g3;
  };

  static std::unique_ptr<Tables> make_tables(std::uint64_t seed);

  template <std::size_t dimension> requires (dimension >= 1 && dimension <= 3)
  std::expected<Float, StarException> sample(std::array<Float, dimension> const& point) const;

  template <PerlinType mode, std::size_t dimension> requires (dimension >= 1 && dimension <= 3)
  std::expected<Float, StarException> sample_octaves(std::array<Float, dimension> point) const;

  Parameters m_parameters;
  std::uint64_t m_seed = 0;
  std::unique_ptr<Tables> m_tables;
};

using PerlinF = Perlin<float>;
using PerlinD = Perlin<double>;

template <finite_classifiable Float>
Float Perlin<Float>::s_curve(Float t) {
  return t * t * (3.0 - 2.0 * t);
}

// Range-test before floor-to-int conversion, including NaN under fast-math.
#pragma float_control(precise, on, push)

template <finite_classifiable Float>
auto Perlin<Float>::lattice_axis(Float const& coordinate) -> LatticeAxis {
  Float value = coordinate;
  constexpr Float lower = std::numeric_limits<int>::min();
  if (!(value >= lower && value < -lower)) [[unlikely]] {
    if (!is_finite(value))
      return {-1, 0, 0, 0};
    value = std::fmod(value, Float(sample_size));
  }
  int cell = static_cast<int>(std::floor(value));
  Float offset = value - cell;
  int lo = cell & (sample_size - 1);
  return {lo, (lo + 1) & (sample_size - 1), offset, Float(offset - 1.0)};
}
#pragma float_control(pop)

template <finite_classifiable Float>
Perlin<Float>::Perlin(Parameters const& parameters, std::uint64_t seed)
  : m_parameters(parameters), m_seed(parameters.type == PerlinType::Uninitialized ? 0 : seed),
    m_tables(parameters.type == PerlinType::Uninitialized ? nullptr : make_tables(seed)) {}

template <finite_classifiable Float>
auto Perlin<Float>::make_parameters(PerlinType type, double const& octaves,
    double const& frequency, double const& amplitude, double const& bias, double const& alpha, double const& beta,
    double const& offset, double const& gain) -> std::expected<Parameters, StarException> {
  auto finite = classify_finite(octaves, frequency, amplitude, bias, alpha, beta, offset, gain);
  if (std::ranges::find(finite, false) != finite.end())
    return std::unexpected(PerlinException("Perlin parameters must be finite"));
  // Existing JSON and selector inputs truncate nonnegative fractional octaves.
  if (octaves < 0 || octaves >= static_cast<double>(std::numeric_limits<int>::max()) + 1)
    return std::unexpected(PerlinException("Perlin octave count is out of range"));
  if constexpr (std::same_as<Float, float>) {
    constexpr double limit = std::numeric_limits<Float>::max();
    if (std::abs(frequency) > limit || std::abs(amplitude) > limit || std::abs(bias) > limit ||
        std::abs(alpha) > limit || std::abs(beta) > limit || std::abs(offset) > limit || std::abs(gain) > limit)
      return std::unexpected(PerlinException("Perlin parameter exceeds the scalar range"));
  }
  Parameters result;
  switch (type) {
    case PerlinType::Uninitialized:
      return result;
    case PerlinType::Perlin:
    case PerlinType::Billow:
    case PerlinType::RidgedMulti:
      break;
    default:
      return std::unexpected(PerlinException("Unknown Perlin type"));
  }
  result.type = type;
  result.octaves = static_cast<unsigned>(octaves);
  result.frequency = static_cast<Float>(frequency);
  result.amplitude = static_cast<Float>(amplitude);
  result.bias = static_cast<Float>(bias);
  result.alpha = static_cast<Float>(alpha);
  result.beta = static_cast<Float>(beta);
  result.offset = static_cast<Float>(offset);
  result.gain = static_cast<Float>(gain);
  if (result.octaves > 1 && result.alpha == 0)
    return std::unexpected(PerlinException("Multiple Perlin octaves require a nonzero alpha"));
  return result;
}

template <finite_classifiable Float>
auto Perlin<Float>::parse_parameters(Json const& config) -> std::expected<Parameters, StarException> {
  auto type_name = config.get_string("type");
  if (!type_name) return std::unexpected(std::move(type_name.error()));
  auto type = enum_value<perlin_type_names>(StringView(*type_name));
  if (!type) return std::unexpected(std::move(type.error()));
  // Older world templates serialized indeterminate noise parameters for this placeholder.
  if (*type == PerlinType::Uninitialized)
    return Parameters{};
  auto octaves = config.get_double("octaves", 1);
  if (!octaves) return std::unexpected(std::move(octaves.error()));
  auto frequency = config.get_double("frequency", 1);
  if (!frequency) return std::unexpected(std::move(frequency.error()));
  auto amplitude = config.get_double("amplitude", 1);
  if (!amplitude) return std::unexpected(std::move(amplitude.error()));
  auto bias = config.get_double("bias", 0);
  if (!bias) return std::unexpected(std::move(bias.error()));
  auto alpha = config.get_double("alpha", 2);
  if (!alpha) return std::unexpected(std::move(alpha.error()));
  auto beta = config.get_double("beta", 2);
  if (!beta) return std::unexpected(std::move(beta.error()));
  auto offset = config.get_double("offset", 1);
  if (!offset) return std::unexpected(std::move(offset.error()));
  auto gain = config.get_double("gain", 2);
  if (!gain) return std::unexpected(std::move(gain.error()));
  return make_parameters(*type, *octaves, *frequency, *amplitude, *bias, *alpha, *beta, *offset, *gain);
}

template <finite_classifiable Float>
std::expected<Perlin<Float>, StarException> Perlin<Float>::create(PerlinType type, double const& octaves,
    double const& frequency, double const& amplitude, double const& bias, double const& alpha, double const& beta,
    std::uint64_t seed) {
  auto parameters = make_parameters(type, octaves, frequency, amplitude, bias, alpha, beta, 1, 2);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  return std::expected<Perlin, StarException>(std::in_place, *parameters, seed);
}

template <finite_classifiable Float>
std::expected<Perlin<Float>, StarException> Perlin<Float>::create(Json const& config, std::uint64_t seed) {
  auto parameters = parse_parameters(config);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  return std::expected<Perlin, StarException>(std::in_place, *parameters, seed);
}

template <finite_classifiable Float>
std::expected<Perlin<Float>, StarException> Perlin<Float>::create(Json const& config) {
  auto seed = config.get_uint("seed");
  if (!seed) return std::unexpected(std::move(seed.error()));
  return create(config, *seed);
}

template <finite_classifiable Float>
Perlin<Float>::Perlin(Perlin const& other)
  : m_parameters(other.m_parameters), m_seed(other.m_seed),
    m_tables(other.m_tables ? std::make_unique<Tables>(*other.m_tables) : nullptr) {}

template <finite_classifiable Float>
Perlin<Float>::Perlin(Perlin&& other) noexcept
  : m_parameters(std::exchange(other.m_parameters, Parameters{})), m_seed(std::exchange(other.m_seed, 0)),
    m_tables(std::move(other.m_tables)) {}

template <finite_classifiable Float>
Perlin<Float>& Perlin<Float>::operator=(Perlin const& other) {
  if (this != &other) {
    if (!other.m_tables)
      m_tables.reset();
    else if (m_tables)
      *m_tables = *other.m_tables;
    else
      m_tables = std::make_unique<Tables>(*other.m_tables);
    m_parameters = other.m_parameters;
    m_seed = other.m_seed;
  }
  return *this;
}

template <finite_classifiable Float>
Perlin<Float>& Perlin<Float>::operator=(Perlin&& other) noexcept {
  if (this != &other) {
    m_parameters = std::exchange(other.m_parameters, Parameters{});
    m_seed = std::exchange(other.m_seed, 0);
    m_tables = std::move(other.m_tables);
  }
  return *this;
}

template <finite_classifiable Float>
std::expected<Float, StarException> Perlin<Float>::get(Float const& x) const {
  return sample(std::array{x});
}

template <finite_classifiable Float>
std::expected<Float, StarException> Perlin<Float>::get(Float const& x, Float const& y) const {
  return sample(std::array{x, y});
}

template <finite_classifiable Float>
std::expected<Float, StarException> Perlin<Float>::get(Float const& x, Float const& y, Float const& z) const {
  return sample(std::array{x, y, z});
}

template <finite_classifiable Float>
template <std::size_t dimension> requires (dimension >= 1 && dimension <= 3)
std::expected<Float, StarException> Perlin<Float>::sample(std::array<Float, dimension> const& point) const {
  switch (m_parameters.type) {
    case PerlinType::Perlin: return sample_octaves<PerlinType::Perlin>(point);
    case PerlinType::Billow: return sample_octaves<PerlinType::Billow>(point);
    case PerlinType::RidgedMulti: return sample_octaves<PerlinType::RidgedMulti>(point);
    default: return std::unexpected(PerlinException("::get called on uninitialized Perlin"));
  }
}

template <finite_classifiable Float>
std::expected<Json, StarException> Perlin<Float>::to_json() const {
  auto type = enum_name<perlin_type_names>(m_parameters.type);
  if (!type) return std::unexpected(std::move(type.error()));
  return JsonObject{
    {"seed", m_seed},
    {"octaves", m_parameters.octaves},
    {"frequency", m_parameters.frequency},
    {"amplitude", m_parameters.amplitude},
    {"bias", m_parameters.bias},
    {"alpha", m_parameters.alpha},
    {"beta", m_parameters.beta},
    {"offset", m_parameters.offset},
    {"gain", m_parameters.gain},
    {"type", String(*type)}
  };
}

template <finite_classifiable Float>
Float Perlin<Float>::noise(std::array<LatticeAxis, 1> const& axes) const {
  auto const& x = axes[0];
  auto const& t = *m_tables;
  Float u = x.near_offset * t.g1[t.p[x.lo]];
  Float v = x.far_offset * t.g1[t.p[x.hi]];
  return lerp(s_curve(x.near_offset), u, v);
}

template <finite_classifiable Float>
Float Perlin<Float>::noise(std::array<LatticeAxis, 2> const& axes) const {
  auto const& [x, y] = axes;
  auto const& t = *m_tables;
  int i = t.p[x.lo], j = t.p[x.hi];
  Float sx = s_curve(x.near_offset);
  auto row = [&](int by, Float ry) {
    auto const& g = t.g2[t.p[i + by]];
    auto const& h = t.g2[t.p[j + by]];
    Float u = x.near_offset * g[0] + ry * g[1];
    Float v = x.far_offset * h[0] + ry * h[1];
    return lerp(sx, u, v);
  };
  Float lower = row(y.lo, y.near_offset), upper = row(y.hi, y.far_offset);
  if constexpr (std::same_as<Float, double>) {
    // Preserve the legacy double kernel's grouping of the final cubic blend.
    Float delta = upper - lower;
    {
#pragma clang fp reassociate(off)
      Float t = y.near_offset;
      return lower + (t * (3.0 - 2.0 * t)) * (t * delta);
    }
  } else {
    return lerp(s_curve(y.near_offset), lower, upper);
  }
}

template <finite_classifiable Float>
Float Perlin<Float>::noise(std::array<LatticeAxis, 3> const& axes) const {
  auto const& [x, y, z] = axes;
  auto const& t = *m_tables;
  int i = t.p[x.lo], j = t.p[x.hi];
  int b00 = t.p[i + y.lo], b10 = t.p[j + y.lo];
  int b01 = t.p[i + y.hi], b11 = t.p[j + y.hi];
  Float sx = s_curve(x.near_offset), sy = s_curve(y.near_offset);
  auto plane = [&]<bool far>(int bz, Float rz) {
    auto row = [&]<bool first>(int left, int right, Float ry) {
      auto const& g = t.g3[left + bz];
      auto const& h = t.g3[right + bz];
      Float u = x.near_offset * g[0] + ry * g[1] + rz * g[2];
      if constexpr (std::same_as<Float, float> && far && first) {
        // This corner's legacy float dot difference added y and z separately.
#pragma clang fp reassociate(off)
        return u + sx * (((x.far_offset * h[0] - u) + ry * h[1]) + rz * h[2]);
      } else {
        Float v = x.far_offset * h[0] + ry * h[1] + rz * h[2];
        return lerp(sx, u, v);
      }
    };
    return lerp(sy, row.template operator()<true>(b00, b10, y.near_offset),
        row.template operator()<false>(b01, b11, y.far_offset));
  };
  return lerp(s_curve(z.near_offset), plane.template operator()<false>(z.lo, z.near_offset),
      plane.template operator()<true>(z.hi, z.far_offset));
}

template <finite_classifiable Float>
auto Perlin<Float>::make_tables(std::uint64_t seed) -> std::unique_ptr<Tables> {
  auto tables = std::make_unique_for_overwrite<Tables>();
  RandomSource random(seed);
  auto component = [&] {
    return static_cast<Float>(static_cast<std::int64_t>(random.uniform_uint(2 * sample_size)) - sample_size) / sample_size;
  };
  auto normalize = []<std::size_t size>(std::array<Float, size>& gradient) requires (size == 2 || size == 3) {
    Float squared = gradient[0] * gradient[0] + gradient[1] * gradient[1];
    if constexpr (size == 3)
      squared += gradient[2] * gradient[2];
    Float length = std::sqrt(squared);
    if (length == 0) {
      gradient.fill(0);
      gradient[0] = 1;
    } else {
      for (Float& value : gradient)
        value /= length;
    }
  };
  for (int i = 0; i < sample_size; ++i) {
    tables->p[i] = i;
    tables->g1[i] = component();
    for (Float& value : tables->g2[i])
      value = component();
    normalize(tables->g2[i]);
    for (Float& value : tables->g3[i])
      value = component();
    normalize(tables->g3[i]);
  }
  // The full-range draws (not Fisher–Yates) are part of the seed format.
  for (int i = sample_size - 1; i > 0; --i)
    std::swap(tables->p[i], tables->p[random.uniform_uint(sample_size - 1)]);
  std::copy_n(tables->p.begin(), sample_size, tables->p.begin() + sample_size);
  std::copy_n(tables->g3.begin(), sample_size, tables->g3.begin() + sample_size);
  return tables;
}

template <finite_classifiable Float>
template <PerlinType mode, std::size_t dimension> requires (dimension >= 1 && dimension <= 3)
std::expected<Float, StarException> Perlin<Float>::sample_octaves(std::array<Float, dimension> point) const {
  Float sum = 0, scale = 1, weight = 1;
  Float factor = m_parameters.frequency;
  for (unsigned octave = 0; octave < m_parameters.octaves; ++octave) {
    std::array<LatticeAxis, dimension> axes;
    for (std::size_t axis = 0; axis < dimension; ++axis) {
      {
        // This product may be nonfinite; never give it fast-math assumptions.
#pragma float_control(precise, on)
        point[axis] *= factor;
      }
      axes[axis] = lattice_axis(point[axis]);
      if (axes[axis].lo < 0) [[unlikely]]
        return std::unexpected(PerlinException("Nonfinite Perlin sampling coordinate"));
    }
    Float value = noise(axes);
    if constexpr (mode == PerlinType::Billow) {
      value = 2.0 * std::fabs(value) - 1.0;
    } else if constexpr (mode == PerlinType::RidgedMulti) {
      value = m_parameters.offset - std::fabs(value);
      value *= value;
      value *= weight;
      weight = std::clamp<Float>(value * m_parameters.gain, 0.0, 1.0);
    }
    sum += value / scale;
    scale *= m_parameters.alpha;
    factor = m_parameters.beta;
  }
  if constexpr (mode == PerlinType::Billow)
    return (sum + 0.5) * m_parameters.amplitude + m_parameters.bias;
  else if constexpr (mode == PerlinType::RidgedMulti)
    return ((sum * 1.25) - 1.0) * m_parameters.amplitude + m_parameters.bias;
  else
    return sum * m_parameters.amplitude + m_parameters.bias;
}

}

export module star.perlin;

export namespace Star {
  using ::Star::PerlinExceptionTag;
  using ::Star::PerlinException;
  using ::Star::PerlinType;
  using ::Star::perlin_type_names;
  using ::Star::Perlin;
  using ::Star::PerlinF;
  using ::Star::PerlinD;
}

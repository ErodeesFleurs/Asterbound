module;
import std;
import star.format;
import star.json;

import star.rect;
import star.image;
import star.color;
import star.list;
import star.vector;
import star.cellular_light_array;
import star.thread;

namespace Star {

struct LightmapExceptionTag {
  static constexpr std::string_view name() { return "LightmapException"; }
};
using LightmapException = StarError<LightmapExceptionTag, StarException>;

class Lightmap {
public:
  Lightmap();
  Lightmap(unsigned width, unsigned height);
  Lightmap(Lightmap const& lightMap);
  Lightmap(Lightmap&& lightMap) noexcept;
  
  Lightmap& operator=(Lightmap const& lightMap);
  Lightmap& operator=(Lightmap&& lightMap) noexcept;

  void swap(Lightmap& other) noexcept;

  operator ImageView();

  [[nodiscard]] std::expected<void, StarException> set(unsigned x, unsigned y, float v);
  [[nodiscard]] std::expected<void, StarException> set(unsigned x, unsigned y, Vec3F const& v);
  [[nodiscard]] std::expected<void, StarException> add(unsigned x, unsigned y, Vec3F const& v);
  [[nodiscard]] std::expected<Vec3F, StarException> get(unsigned x, unsigned y) const;

  bool empty() const;

  Vec2U size() const;
  unsigned width() const;
  unsigned height() const;
  std::span<float> data();
  std::span<float const> data() const;

private:
  friend class CellularLightingCalculator;
  // Only the calculator skips initialization; it overwrites every RGB channel.
  [[nodiscard]] std::expected<void, StarException> resizeForOverwrite(unsigned width, unsigned height);
  std::size_t len() const;

  std::unique_ptr<float[]> m_data;
  std::size_t m_capacity = 0;
  unsigned m_width;
  unsigned m_height;
};

inline std::expected<void, StarException> Lightmap::set(unsigned x, unsigned y, float v) {
  if (x >= m_width || y >= m_height)
    return std::unexpected(LightmapException(Star::format("[{}, {}] out of range in Lightmap::set", x, y)));
  auto pixels = data().subspan((std::size_t(y) * m_width + x) * 3, 3);
  pixels[0] = pixels[1] = pixels[2] = v;
  return {};
}

inline std::expected<void, StarException> Lightmap::set(unsigned x, unsigned y, Vec3F const& v) {
  if (x >= m_width || y >= m_height)
    return std::unexpected(LightmapException(Star::format("[{}, {}] out of range in Lightmap::set", x, y)));
  auto pixels = data().subspan((std::size_t(y) * m_width + x) * 3, 3);
  pixels[0] = v.x();
  pixels[1] = v.y();
  pixels[2] = v.z();
  return {};
}

inline std::expected<void, StarException> Lightmap::add(unsigned x, unsigned y, Vec3F const& v) {
  if (x >= m_width || y >= m_height)
    return std::unexpected(LightmapException(Star::format("[{}, {}] out of range in Lightmap::add", x, y)));
  auto pixels = data().subspan((std::size_t(y) * m_width + x) * 3, 3);
  pixels[0] += v.x();
  pixels[1] += v.y();
  pixels[2] += v.z();
  return {};
}

inline std::expected<Vec3F, StarException> Lightmap::get(unsigned x, unsigned y) const {
  if (x >= m_width || y >= m_height)
    return std::unexpected(LightmapException(Star::format("[{}, {}] out of range in Lightmap::get", x, y)));
  auto pixels = data().subspan((std::size_t(y) * m_width + x) * 3, 3);
  return Vec3F(pixels[0], pixels[1], pixels[2]);
}


inline bool Lightmap::empty() const {
  return m_width == 0 || m_height == 0;
}

inline Vec2U Lightmap::size() const {
  return { m_width, m_height };
}

inline unsigned Lightmap::width() const {
  return m_width;
}

inline unsigned Lightmap::height() const {
  return m_height;
}

inline std::span<float> Lightmap::data() {
  return {m_data.get(), len()};
}

inline std::span<float const> Lightmap::data() const {
  return {m_data.get(), len()};
}

inline std::size_t Lightmap::len() const {
  return std::size_t(m_width) * m_height * 3;
}

// Produce lighting values from an integral cellular grid.  Allows for floating
// positional point and cellular light sources, as well as pre-lighting cells
// individually.
class CellularLightingCalculator {
public:
  explicit CellularLightingCalculator();
  explicit CellularLightingCalculator(bool monochrome);

  using Cell = ColoredCellularLightArray::Cell;

  [[nodiscard]] std::expected<void, StarException> set_monochrome(bool monochrome);

  [[nodiscard]] std::expected<void, StarException> set_parameters(Json const& config);

  // Call 'begin' to start a calculation for the given region
  [[nodiscard]] std::expected<void, StarException> begin(RectI const& query_region);

  // Once begin is called, this will return the region that could possibly
  // affect the target calculation region.  All lighting values should be set
  // for the given calculation region before calling 'calculate'.
  RectI calculation_region() const;

  std::size_t base_index_for(Vec2I const& position);

  void set_cell_index(std::size_t cell_index, Vec3F const& light, bool obstacle);

  void add_spread_light(Vec2F const& position, Vec3F const& light);
  void add_point_light(Vec2F const& position, Vec3F const& light, float beam, float beam_angle, float beam_ambience, bool as_spread = false);

  // Finish the calculation, and put the resulting color data in the given
  // output image.  The image will be reset to the size of the region given in
  // the call to 'begin', and formatted as RGB24.
  void calculate(Image& output);
  // Same as above, but the color data in a float buffer instead.
  [[nodiscard]] std::expected<void, StarException> calculate(Lightmap& output);

  void setup_image(Image& image, PixelFormat format = PixelFormat::RGB24) const;
private:
  std::optional<LightParameters> m_parameters;
  std::optional<float> m_brightness_limit;
  using LightArray = std::variant<ColoredCellularLightArray, ScalarCellularLightArray>;
  LightArray m_light_array;
  RectI m_query_region;
  RectI m_calculation_region;
};

// Produce light intensity values using the same algorithm as
// CellularLightingCalculator.  Only calculates a single point at a time, and
// uses scalar lights with no color calculation.
class CellularLightIntensityCalculator {
public:
  using Cell = ScalarCellularLightArray::Cell;

  [[nodiscard]] std::expected<void, StarException> set_parameters(Json const& config);

  [[nodiscard]] std::expected<void, StarException> begin(Vec2F const& query_position);

  RectI calculation_region() const;

  void set_cell(Vec2I const& position, Cell const& cell);
  void set_cell_column(Vec2I const& position, std::span<Cell const> cells);

  void add_spread_light(Vec2F const& position, float light);
  void add_point_light(Vec2F const& position, float light, float beam, float beam_angle, float beam_ambience);

  float calculate();

private:
  ScalarCellularLightArray m_light_array;
  Vec2F m_query_position;
  RectI m_query_region;
  RectI m_calculation_region;
};

inline std::size_t CellularLightingCalculator::base_index_for(Vec2I const& position) {
  return std::size_t(position[0] - m_calculation_region.x_min()) * m_calculation_region.height() + (position[1] - m_calculation_region.y_min());
}

inline void CellularLightingCalculator::set_cell_index(std::size_t cell_index, Vec3F const& light, bool obstacle) {
  if (std::holds_alternative<ScalarCellularLightArray>(m_light_array)) {
#if defined(__clang__)
#pragma clang fp reassociate(off)
#endif
    // Preserve the channel grouping and reciprocal used by the native monochrome gather.
    float intensity = ((light[0] + light[2]) + light[1]) * (1.0f / 3.0f);
    std::get<ScalarCellularLightArray>(m_light_array).cell_at_index(cell_index) =
      ScalarCellularLightArray::Cell{.light = intensity, .obstacle = obstacle};
  } else {
    std::get<ColoredCellularLightArray>(m_light_array).cell_at_index(cell_index) =
      ColoredCellularLightArray::Cell{.light = light, .obstacle = obstacle};
  }
}

}

export module star.cellular_lighting;

export namespace Star {
  using ::Star::LightmapExceptionTag;
  using ::Star::LightmapException;
  using ::Star::Lightmap;
  using ::Star::CellularLightingCalculator;
  using ::Star::CellularLightIntensityCalculator;
}

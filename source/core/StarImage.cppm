module;
import std;
import star.format;
import star.string;
import star.vector;
import star.io_device;

namespace Star {

class Logger;

enum class PixelFormat : std::uint8_t { RGB24, RGBA32, BGR24, BGRA32, RGB_F, RGBA_F };

constexpr std::uint8_t bytes_per_pixel(PixelFormat format) noexcept {
  switch (format) {
  case PixelFormat::RGB24:
  case PixelFormat::BGR24: return 3;
  case PixelFormat::RGBA32:
  case PixelFormat::BGRA32: return 4;
  case PixelFormat::RGB_F: return 12;
  default: return 16;
  }
}
constexpr std::uint8_t bits_per_pixel(PixelFormat format) noexcept { return bytes_per_pixel(format) * 8; }

struct ImageExceptionTag {
  static constexpr std::string_view name() { return "ImageException"; }
};
using ImageException = StarError<ImageExceptionTag, StarException>;

// Traversal accepts void or the existing default-success/error result protocol.
// Check the actual lvalue call and contextual failure conversion, not predicate.
template <typename Result>
concept pixel_result = std::is_void_v<Result> || (std::is_object_v<Result> && requires(Result& result) {
                         Result{};
                         !result ? 0 : 0;
                         result.error();
                         Result(std::unexpected(std::move(result.error())));
                       });

template <typename Callback, typename Pixel>
concept pixel_callback = std::invocable<Callback&, unsigned, unsigned, Pixel>
  && pixel_result<std::invoke_result_t<Callback&, unsigned, unsigned, Pixel>>;

template <typename Callback>
concept pixel_reader = pixel_callback<Callback, Vec4B const&>;

template <typename Callback>
concept pixel_writer = pixel_callback<Callback, Vec4B&>;

// Tightly packed row-major storage, with (0, 0) at the lower-left corner.
class Image {
public:
  [[nodiscard]] static std::expected<Image, StarException> read_png(Logger& logger, std::shared_ptr<IODevice> device);
  [[nodiscard]] static std::expected<bool, StarException> is_png(std::shared_ptr<IODevice> device);
  [[nodiscard]] static std::expected<std::tuple<Vec2U, PixelFormat>, StarException>
  read_png_metadata(Logger& logger, std::shared_ptr<IODevice> device);

  // Validate a layout without allocating; the result is a byte count.
  [[nodiscard]] static std::expected<std::size_t, StarException> size_bytes(Vec2U size, PixelFormat format);
  static Image filled(Vec2U size, Vec4B color, PixelFormat format = PixelFormat::RGBA32);
  // Like make_unique_for_overwrite: every byte must be written before reading.
  static Image for_overwrite(Vec2U size, PixelFormat format = PixelFormat::RGBA32);

  Image() noexcept : Image(PixelFormat::RGBA32) {}
  Image(PixelFormat format) noexcept;
  Image(Vec2U size, PixelFormat format = PixelFormat::RGBA32);
  Image(unsigned width, unsigned height, PixelFormat format = PixelFormat::RGBA32);
  ~Image();
  Image(Image const& image);
  Image(Image&& image) noexcept;
  Image& operator=(Image const& image);
  Image& operator=(Image&& image) noexcept;

  std::uint8_t bits_per_pixel() const noexcept { return Star::bits_per_pixel(m_pixelFormat); }
  std::uint8_t bytes_per_pixel() const noexcept { return Star::bytes_per_pixel(m_pixelFormat); }
  unsigned width() const noexcept { return m_width; }
  unsigned height() const noexcept { return m_height; }
  Vec2U size() const noexcept { return {m_width, m_height}; }
  bool empty() const noexcept { return m_width == 0 || m_height == 0; }
  PixelFormat pixel_format() const noexcept { return m_pixelFormat; }
  std::size_t size_bytes() const noexcept { return std::size_t(m_width) * m_height * bytes_per_pixel(); }
  std::span<std::uint8_t const> data() const noexcept { return {m_data.get(), size_bytes()}; }
  std::span<std::uint8_t> data() noexcept { return {m_data.get(), size_bytes()}; }

  // Changed layouts are zeroed. An unchanged extent/format preserves pixels.
  void reset(Vec2U size, std::optional<PixelFormat> format = {});
  void reset(unsigned width, unsigned height, std::optional<PixelFormat> format = {});
  void fill(Vec3B const& color);
  void fill(Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> fill_rect(Vec2U const& pos, Vec2U const& size, Vec3B const& color);
  [[nodiscard]] std::expected<void, StarException> fill_rect(Vec2U const& pos, Vec2U const& size, Vec4B const& color);

  // Raw internal channels; missing alpha is 255. Invalid coordinates are errors.
  [[nodiscard]] std::expected<void, StarException> set(Vec2U const& pos, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> set(Vec2U const& pos, Vec3B const& color);
  [[nodiscard]] std::expected<Vec4B, StarException> get(Vec2U const& pos) const;
  [[nodiscard]] std::expected<void, StarException> set_rgb(Vec2U const& pos, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> set_rgb(Vec2U const& pos, Vec3B const& color);
  [[nodiscard]] std::expected<Vec4B, StarException> get_rgb(Vec2U const& pos) const;
  // Empty images clamp to transparent black.
  Vec4B clamp(Vec2I const& pos) const;
  Vec4B clamp_rgb(Vec2I const& pos) const;

  [[nodiscard]] std::expected<void, StarException> set(unsigned x, unsigned y, Vec4B const& color) {
    return set({x, y}, color);
  }
  [[nodiscard]] std::expected<void, StarException> set(unsigned x, unsigned y, Vec3B const& color) {
    return set({x, y}, color);
  }
  [[nodiscard]] std::expected<Vec4B, StarException> get(unsigned x, unsigned y) const { return get({x, y}); }
  [[nodiscard]] std::expected<void, StarException> set_rgb(unsigned x, unsigned y, Vec4B const& color) {
    return set_rgb({x, y}, color);
  }
  [[nodiscard]] std::expected<void, StarException> set_rgb(unsigned x, unsigned y, Vec3B const& color) {
    return set_rgb({x, y}, color);
  }
  [[nodiscard]] std::expected<Vec4B, StarException> get_rgb(unsigned x, unsigned y) const { return get_rgb({x, y}); }
  Vec4B clamp(int x, int y) const { return clamp({x, y}); }
  Vec4B clamp_rgb(int x, int y) const { return clamp_rgb({x, y}); }

  // Unchecked raw access for exactly four- or three-byte pixels.
  void set32(Vec2U const& pos, Vec4B const& color) noexcept { set32(pos[0], pos[1], color); }
  void set32(unsigned x, unsigned y, Vec4B const& color) noexcept;
  Vec4B get32(unsigned x, unsigned y) const noexcept;
  void set24(Vec2U const& pos, Vec3B const& color) noexcept { set24(pos[0], pos[1], color); }
  void set24(unsigned x, unsigned y, Vec3B const& color) noexcept;
  Vec3B get24(unsigned x, unsigned y) const noexcept;

  // Callbacks are borrowed lvalues. A failed mutable pixel is not committed.
  template <pixel_reader Callback> [[nodiscard]] auto for_each_pixel(Callback&& callback) const {
    return for_each_pixel_impl<false>(*this, callback);
  }
  template <pixel_writer Callback> [[nodiscard]] auto for_each_pixel(Callback&& callback) {
    return for_each_pixel_impl<true>(*this, callback);
  }

  [[nodiscard]] std::expected<Image, StarException> sub_image(Vec2U const& pos, Vec2U const& size) const;
  [[nodiscard]] std::expected<void, StarException> copy_into(Vec2U const& pos, Image const& image);
  [[nodiscard]] std::expected<void, StarException> draw_into(Vec2U const& pos, Image const& image);
  // Retains the existing raw-channel conversion, including BGR ordering.
  Image convert(PixelFormat format) const;
  [[nodiscard]] std::expected<void, StarException> write_png(Logger& logger, std::shared_ptr<IODevice> device) const;

private:
  void reallocate(Vec2U size, PixelFormat format, bool initialize);

  template <bool Writable, typename Callback>
    requires(Writable ? pixel_writer<Callback> : pixel_reader<Callback>)
  static auto for_each_pixel_impl(std::conditional_t<Writable, Image, Image const>& image, Callback& callback);

  std::unique_ptr<std::uint8_t[]> m_data;
  unsigned m_width;
  unsigned m_height;
  PixelFormat m_pixelFormat;
};

inline void Image::set32(unsigned x, unsigned y, Vec4B const& color) noexcept {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed(m_data && x < m_width && y < m_height);
    Star::star_assert_failed(bytes_per_pixel() == 4);
  }
  auto pixel = std::next(data().begin(), (std::size_t(y) * m_width + x) * 4);
  for (unsigned channel = 0; channel != 4; ++channel)
    pixel[channel] = color[channel];
}

inline Vec4B Image::get32(unsigned x, unsigned y) const noexcept {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed(m_data && x < m_width && y < m_height);
    Star::star_assert_failed(bytes_per_pixel() == 4);
  }
  auto pixel = std::next(data().begin(), (std::size_t(y) * m_width + x) * 4);
  return {pixel[0], pixel[1], pixel[2], pixel[3]};
}

inline void Image::set24(unsigned x, unsigned y, Vec3B const& color) noexcept {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed(m_data && x < m_width && y < m_height);
    Star::star_assert_failed(bytes_per_pixel() == 3);
  }
  auto pixel = std::next(data().begin(), (std::size_t(y) * m_width + x) * 3);
  for (unsigned channel = 0; channel != 3; ++channel)
    pixel[channel] = color[channel];
}

inline Vec3B Image::get24(unsigned x, unsigned y) const noexcept {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed(m_data && x < m_width && y < m_height);
    Star::star_assert_failed(bytes_per_pixel() == 3);
  }
  auto pixel = std::next(data().begin(), (std::size_t(y) * m_width + x) * 3);
  return {pixel[0], pixel[1], pixel[2]};
}

template <bool Writable, typename Callback>
  requires(Writable ? pixel_writer<Callback> : pixel_reader<Callback>)
auto Image::for_each_pixel_impl(std::conditional_t<Writable, Image, Image const>& image, Callback& callback) {
  using Pixel = std::conditional_t<Writable, Vec4B&, Vec4B const&>;
  using Result = std::invoke_result_t<Callback&, unsigned, unsigned, Pixel>;
  for (unsigned y = 0; y < image.m_height; ++y) {
    for (unsigned x = 0; x < image.m_width; ++x) {
      Vec4B pixel = image.bytes_per_pixel() == 4 ? image.get32(x, y) : Vec4B(image.get24(x, y), 255);
      Pixel argument = pixel;
      if constexpr (std::is_void_v<Result>) {
        std::invoke(callback, x, y, argument);
      } else {
        auto result = std::invoke(callback, x, y, argument);
        if (!result)
          return Result(std::unexpected(std::move(result.error())));
      }
      if constexpr (Writable) {
        // Re-read storage after invoking user code, as the old traversal did.
        if (image.bytes_per_pixel() == 4)
          image.set32(x, y, pixel);
        else
          image.set24(x, y, pixel.vec3());
      }
    }
  }
  if constexpr (!std::is_void_v<Result>)
    return Result{};
}

struct ImageView {
  bool empty() const noexcept { return size.x() == 0 || size.y() == 0; }
  ImageView() = default;
  ImageView(Image const& image);
  Vec2U size{0, 0};
  std::span<std::uint8_t const> data;
  PixelFormat format = PixelFormat::RGB24;
};

}

export module star.image;

export namespace Star {
using ::Star::bits_per_pixel;
using ::Star::bytes_per_pixel;
using ::Star::Image;
using ::Star::ImageException;
using ::Star::ImageExceptionTag;
using ::Star::ImageView;
using ::Star::pixel_callback;
using ::Star::pixel_reader;
using ::Star::pixel_result;
using ::Star::pixel_writer;
using ::Star::PixelFormat;
}

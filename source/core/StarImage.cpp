#include <png.h>
import std;
import star.format;
import star.io_device;
import star.vector;
import star.image;
import star.logging;


import star.memory;
import star.math_common;
import star.algorithm;
namespace Star {

namespace {

struct PngState {
  Logger& logger;
  IODevice& device;
  bool writing;
  png_structp png = nullptr;
  png_infop info = nullptr;
  png_infop end = nullptr;
  std::optional<StarException> error{};

  ~PngState() {
    if (writing)
      png_destroy_write_struct(&png, &info);
    else
      png_destroy_read_struct(&png, &info, &end);
  }

  std::expected<void, StarException> check(bool success, std::string_view message) {
    if (error)
      return std::unexpected(std::move(*error));
    if (!success)
      return std::unexpected(ImageException(std::string(message)));
    return {};
  }
};

void logPngMessage(PngState& state, std::string_view kind, std::string_view message) noexcept {
  auto name = state.device.device_name();
  if (!name) {
    state.error = std::move(name.error());
    return;
  }
  auto logged = state.logger.debug("PNG {} in file: '{}', {}", kind, *name, message);
  if (!logged)
    state.error = std::move(logged.error());
}

void pngError(png_structp png, png_const_charp message) noexcept {
  auto& state = *static_cast<PngState*>(png_get_error_ptr(png));
  logPngMessage(state, "error", message);
  if (!state.error)
    state.error = ImageException(std::string(message));
  png_longjmp(png, 1);
}

void pngWarning(png_structp png, png_const_charp message) noexcept {
  auto& state = *static_cast<PngState*>(png_get_error_ptr(png));
  logPngMessage(state, "warning", message);
  if (state.error)
    png_longjmp(png, 1);
}

void readPngData(png_structp png, png_bytep data, png_size_t length) noexcept {
  auto& state = *static_cast<PngState*>(png_get_io_ptr(png));
  {
    auto result = state.device.read_full({reinterpret_cast<char*>(data), length});
    if (!result)
      state.error = std::move(result.error());
  }
  // Destroy the result before jumping back through the C decoder.
  if (state.error)
    png_longjmp(png, 1);
}

void writePngData(png_structp png, png_bytep data, png_size_t length) noexcept {
  auto& state = *static_cast<PngState*>(png_get_io_ptr(png));
  {
    auto result = state.device.write_full({reinterpret_cast<char*>(data), length});
    if (!result)
      state.error = std::move(result.error());
  }
  if (state.error)
    png_longjmp(png, 1);
}

void flushPngData(png_structp) noexcept {}

// Each jump target lives in a separate frame with only trivial locals. Owners,
// Images, row arrays and diagnostics live in the caller, never across a jump.
bool createPngInfo(PngState& state) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  state.info = png_create_info_struct(state.png);
  if (!state.info)
    return false;
  if (!state.writing) {
    state.end = png_create_info_struct(state.png);
    if (!state.end)
      return false;
  }
  return true;
}

std::expected<void, StarException> initializePng(PngState& state) {
  if (state.writing)
    state.png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &state, pngError, pngWarning);
  else
    state.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &state, pngError, pngWarning);
  auto created = state.check(state.png != nullptr, "Internal libPNG error");
  if (!created)
    return std::unexpected(std::move(created.error()));
  return state.check(createPngInfo(state), "Internal libPNG error");
}

struct PngMetadata {
  png_uint_32 width;
  png_uint_32 height;
  png_uint_32 bitdepth;
  png_uint_32 channels;
};

bool readPngInfo(PngState& state, PngMetadata& metadata, bool strip16) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  png_set_read_fn(state.png, &state, readPngData);
  png_set_sig_bytes(state.png, 8);
  png_read_info(state.png, state.info);

  metadata.width = png_get_image_width(state.png, state.info);
  metadata.height = png_get_image_height(state.png, state.info);
  metadata.bitdepth = png_get_bit_depth(state.png, state.info);
  metadata.channels = png_get_channels(state.png, state.info);
  auto colorType = png_get_color_type(state.png, state.info);

  if (colorType == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(state.png);
    metadata.channels = 3;
    metadata.bitdepth = 8;
  }
  if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
    if (metadata.bitdepth < 8) {
      png_set_expand_gray_1_2_4_to_8(state.png);
      metadata.bitdepth = 8;
    }
    png_set_gray_to_rgb(state.png);
    metadata.channels = colorType == PNG_COLOR_TYPE_GRAY_ALPHA ? 4 : 3;
  }
  if (png_get_valid(state.png, state.info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(state.png);
    metadata.channels += 1;
  }
  if (strip16 && metadata.bitdepth == 16) {
    png_set_strip_16(state.png);
    metadata.bitdepth = 8;
  }
  return true;
}

bool readPngRows(PngState& state, png_bytepp rows) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  png_read_image(state.png, rows);
  return true;
}

bool writePngRows(PngState& state, unsigned width, unsigned height, unsigned channels, png_bytepp rows) {
  if (setjmp(png_jmpbuf(state.png)))
    return false;
  png_set_IHDR(state.png, state.info, width, height, 8,
      channels == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
      PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_write_fn(state.png, &state, writePngData, flushPngData);
  png_set_rows(state.png, state.info, rows);
  png_write_png(state.png, state.info, PNG_TRANSFORM_IDENTITY, nullptr);
  return true;
}

std::expected<void, StarException> readPngSignature(IODevice& device) {
  png_byte header[8]{};
  auto result = device.read_full({reinterpret_cast<char*>(header), sizeof(header)});
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (png_sig_cmp(header, 0, sizeof(header))) {
    auto name = device.device_name();
    if (!name)
      return std::unexpected(std::move(name.error()));
    return std::unexpected(ImageException(strf("File {} is not a png image!", *name)));
  }
  return {};
}

}

std::expected<bool, StarException> Image::isPng(std::shared_ptr<IODevice> device) {
  png_byte header[8]{};
  auto read = device->read_absolute(0, {reinterpret_cast<char*>(header), sizeof(header)});
  if (!read)
    return std::unexpected(std::move(read.error()));
  return *read == sizeof(header) && !png_sig_cmp(header, 0, sizeof(header));
}

std::expected<Image, StarException> Image::readPng(Logger& logger, std::shared_ptr<IODevice> device) {
  auto signature = readPngSignature(*device);
  if (!signature)
    return std::unexpected(std::move(signature.error()));
  PngState state{logger, *device, false};
  auto initialized = initializePng(state);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  PngMetadata metadata{};
  auto info = state.check(readPngInfo(state, metadata, true), "Internal error reading png.");
  if (!info)
    return std::unexpected(std::move(info.error()));

  if (metadata.bitdepth != 8 || (metadata.channels != 3 && metadata.channels != 4))
    return std::unexpected(ImageException("Unsupported PNG pixel format"));

  Image image(metadata.width, metadata.height, metadata.channels == 3 ? PixelFormat::RGB24 : PixelFormat::RGBA32);
  auto rows = std::make_unique<png_bytep[]>(metadata.height);
  std::size_t stride = static_cast<std::size_t>(metadata.width) * metadata.channels;
  for (std::size_t i = 0; i < metadata.height; ++i)
    rows[i] = image.data().data() + (metadata.height - i - 1) * stride;
  auto read = state.check(readPngRows(state, rows.get()), "Internal error reading png.");
  if (!read)
    return std::unexpected(std::move(read.error()));
  return image;
}

std::expected<std::tuple<Vec2U, PixelFormat>, StarException> Image::readPngMetadata(Logger& logger, std::shared_ptr<IODevice> device) {
  auto signature = readPngSignature(*device);
  if (!signature)
    return std::unexpected(std::move(signature.error()));
  PngState state{logger, *device, false};
  auto initialized = initializePng(state);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  PngMetadata metadata{};
  auto info = state.check(readPngInfo(state, metadata, false), "Internal error reading png.");
  if (!info)
    return std::unexpected(std::move(info.error()));
  return std::make_tuple(Vec2U{metadata.width, metadata.height},
      metadata.channels == 3 ? PixelFormat::RGB24 : PixelFormat::RGBA32);
}

Image Image::filled(Vec2U size, Vec4B color, PixelFormat pf) {
  Image image(size, pf);
  image.fill(color);
  return image;
}

Image::Image(PixelFormat pf)
  : m_data(nullptr), m_width(0), m_height(0), m_pixelFormat(pf) {}

Image::Image(Vec2U size, PixelFormat pf)
  : Image(size[0], size[1], pf) {}

Image::Image(unsigned width, unsigned height, PixelFormat pf)
  : Image(pf) {
  reset(width, height, pf);
}

Image::~Image() {
  if (m_data)
    Star::free(m_data);
}

Image::Image(Image const& image) : Image() {
  operator=(image);
}

Image::Image(Image&& image) : Image() {
  operator=(std::move(image));
}

Image& Image::operator=(Image const& image) {
  reset(image.m_width, image.m_height, image.m_pixelFormat);
  std::memcpy(data().data(), image.data().data(), data().size());
  return *this;
}

Image& Image::operator=(Image&& image) {
  reset(0, 0, m_pixelFormat);

  m_data = std::exchange(image.m_data, nullptr);
  m_width = std::exchange(image.m_width, 0);
  m_height = std::exchange(image.m_height, 0);
  m_pixelFormat = std::exchange(image.m_pixelFormat, PixelFormat{});
  return *this;
}

void Image::reset(Vec2U size, std::optional<PixelFormat> pf) {
  reset(size[0], size[1], pf);
}

void Image::reset(unsigned width, unsigned height, std::optional<PixelFormat> pf) {
  if (!pf)
    pf = m_pixelFormat;

  if (m_width == width && m_height == height && m_pixelFormat == *pf)
    return;

  std::size_t imageSize = width * height * Star::bytesPerPixel(*pf);
  if (imageSize == 0) {
    if (m_data) {
      Star::free(m_data);
      m_data = nullptr;
    }
  } else {
    std::uint8_t* newData = nullptr;
    if (!m_data)
      newData = static_cast<std::uint8_t*>(Star::malloc(imageSize));
    else
      newData = static_cast<std::uint8_t*>(Star::realloc(m_data, imageSize));

    if (!newData)
      std::terminate();

    m_data = newData;
    std::memset(m_data, 0, imageSize);
  }

  m_pixelFormat = *pf;
  m_width = width;
  m_height = height;
}

void Image::fill(Vec3B const& c) {
  if (bitsPerPixel() == 24) {
    for (unsigned y = 0; y < m_height; ++y)
      for (unsigned x = 0; x < m_width; ++x)
        set24(x, y, c);
  } else {
    for (unsigned y = 0; y < m_height; ++y)
      for (unsigned x = 0; x < m_width; ++x)
        set32(x, y, Vec4B(c, 255));
  }
}

void Image::fill(Vec4B const& c) {
  if (bitsPerPixel() == 24) {
    for (unsigned y = 0; y < m_height; ++y)
      for (unsigned x = 0; x < m_width; ++x)
        set24(x, y, c.vec3());
  } else {
    for (unsigned y = 0; y < m_height; ++y)
      for (unsigned x = 0; x < m_width; ++x)
        set32(x, y, c);
  }
}

std::expected<void, StarException> Image::fillRect(Vec2U const& pos, Vec2U const& size, Vec3B const& c) {
  for (unsigned y = pos[1]; y < pos[1] + size[1] && y < m_height; ++y)
    for (unsigned x = pos[0]; x < pos[0] + size[0] && x < m_width; ++x) {
      auto result = set(Vec2U(x, y), c);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

std::expected<void, StarException> Image::fillRect(Vec2U const& pos, Vec2U const& size, Vec4B const& c) {
  for (unsigned y = pos[1]; y < pos[1] + size[1] && y < m_height; ++y)
    for (unsigned x = pos[0]; x < pos[0] + size[0] && x < m_width; ++x) {
      auto result = set(Vec2U(x, y), c);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

std::expected<void, StarException> Image::set(Vec2U const& pos, Vec4B const& c) {
  if (pos[0] >= m_width || pos[1] >= m_height) {
    return std::unexpected(ImageException(strf("{} out of range in Image::set", pos)));
  } else if (bytesPerPixel() == 4) {
    std::size_t offset = pos[1] * m_width * 4 + pos[0] * 4;
    m_data[offset] = c[0];
    m_data[offset + 1] = c[1];
    m_data[offset + 2] = c[2];
    m_data[offset + 3] = c[3];
  } else if (bytesPerPixel() == 3) {
    std::size_t offset = pos[1] * m_width * 3 + pos[0] * 3;
    m_data[offset] = c[0];
    m_data[offset + 1] = c[1];
    m_data[offset + 2] = c[2];
  }
  return {};
}

std::expected<void, StarException> Image::set(Vec2U const& pos, Vec3B const& c) {
  if (pos[0] >= m_width || pos[1] >= m_height) {
    return std::unexpected(ImageException(strf("{} out of range in Image::set", pos)));
  } else if (bytesPerPixel() == 4) {
    std::size_t offset = pos[1] * m_width * 4 + pos[0] * 4;
    m_data[offset] = c[0];
    m_data[offset + 1] = c[1];
    m_data[offset + 2] = c[2];
    m_data[offset + 3] = 255;
  } else if (bytesPerPixel() == 3) {
    std::size_t offset = pos[1] * m_width * 3 + pos[0] * 3;
    m_data[offset] = c[0];
    m_data[offset + 1] = c[1];
    m_data[offset + 2] = c[2];
  }
  return {};
}

std::expected<Vec4B, StarException> Image::get(Vec2U const& pos) const {
  Vec4B c;
  if (pos[0] >= m_width || pos[1] >= m_height) {
    return std::unexpected(ImageException(strf("{} out of range in Image::get", pos)));
  } else if (bytesPerPixel() == 4) {
    std::size_t offset = pos[1] * m_width * 4 + pos[0] * 4;
    c[0] = m_data[offset];
    c[1] = m_data[offset + 1];
    c[2] = m_data[offset + 2];
    c[3] = m_data[offset + 3];
  } else if (bytesPerPixel() == 3) {
    std::size_t offset = pos[1] * m_width * 3 + pos[0] * 3;
    c[0] = m_data[offset];
    c[1] = m_data[offset + 1];
    c[2] = m_data[offset + 2];
    c[3] = 255;
  }
  return c;
}

std::expected<void, StarException> Image::setrgb(Vec2U const& pos, Vec4B const& c) {
  if (m_pixelFormat == PixelFormat::BGR24 || m_pixelFormat == PixelFormat::BGRA32)
    return set(pos, Vec4B{c[2], c[1], c[0], c[3]});
  return set(pos, c);
}

std::expected<void, StarException> Image::setrgb(Vec2U const& pos, Vec3B const& c) {
  if (m_pixelFormat == PixelFormat::BGR24 || m_pixelFormat == PixelFormat::BGRA32)
    return set(pos, Vec3B{c[2], c[1], c[0]});
  return set(pos, c);
}

std::expected<Vec4B, StarException> Image::getrgb(Vec2U const& pos) const {
  auto c = get(pos);
  if (!c)
    return std::unexpected(std::move(c.error()));
  if (m_pixelFormat == PixelFormat::BGR24 || m_pixelFormat == PixelFormat::BGRA32)
    return Vec4B{(*c)[2], (*c)[1], (*c)[0], (*c)[3]};
  return *c;
}

Vec4B Image::clamp(Vec2I const& pos) const {
  Vec4B c;
  unsigned x = static_cast<unsigned>(Star::clamp<int>(pos[0], 0, m_width - 1));
  unsigned y = static_cast<unsigned>(Star::clamp<int>(pos[1], 0, m_height - 1));
  if (m_width == 0 || m_height == 0) {
    return {0, 0, 0, 0};
  } else if (bytesPerPixel() == 4) {
    std::size_t offset = y * m_width * 4 + x * 4;
    c[0] = m_data[offset];
    c[1] = m_data[offset + 1];
    c[2] = m_data[offset + 2];
    c[3] = m_data[offset + 3];
  } else if (bytesPerPixel() == 3) {
    std::size_t offset = y * m_width * 3 + x * 3;
    c[0] = m_data[offset];
    c[1] = m_data[offset + 1];
    c[2] = m_data[offset + 2];
    c[3] = 255;
  }
  return c;
}

Vec4B Image::clamprgb(Vec2I const& pos) const {
  auto c = clamp(pos);
  if (m_pixelFormat == PixelFormat::BGR24 || m_pixelFormat == PixelFormat::BGRA32)
    return Vec4B{c[2], c[1], c[0], c[3]};
  else
    return c;
}

std::expected<Image, StarException> Image::subImage(Vec2U const& pos, Vec2U const& size) const {
  if (pos[0] + size[0] > m_width || pos[1] + size[1] > m_height)
    return std::unexpected(ImageException(strf("call to subImage with pos {} size {} out of image bounds ({}, {})", pos, size, m_width, m_height)));

  Image sub(size[0], size[1], m_pixelFormat);

  for (unsigned y = 0; y < size[1]; ++y) {
    for (unsigned x = 0; x < size[0]; ++x) {
      auto pixel = get(pos + Vec2U(x, y));
      if (!pixel)
        return std::unexpected(std::move(pixel.error()));
      auto assigned = sub.set({x, y}, *pixel);
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }

  return sub;
}

std::expected<void, StarException> Image::copyInto(Vec2U const& min, Image const& image) {
  Vec2U max = (min + image.size()).piecewise_min(size());

  for (unsigned y = min[1]; y < max[1]; ++y) {
    for (unsigned x = min[0]; x < max[0]; ++x) {
      auto pixel = image.get(Vec2U(x, y) - min);
      if (!pixel)
        return std::unexpected(std::move(pixel.error()));
      auto assigned = set(x, y, *pixel);
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }
  return {};
}

std::expected<void, StarException> Image::drawInto(Vec2U const& min, Image const& image) {
  Vec2U max = (min + image.size()).piecewise_min(size());

  for (unsigned y = min[1]; y < max[1]; ++y) {
    for (unsigned x = min[0]; x < max[0]; ++x) {
      auto destResult = get(Vec2U(x, y));
      if (!destResult)
        return std::unexpected(std::move(destResult.error()));
      auto srcResult = image.get(Vec2U(x, y) - min);
      if (!srcResult)
        return std::unexpected(std::move(srcResult.error()));
      Vec4B dest = *destResult;
      Vec4B src = *srcResult;

      Vec3U destMultiplied = Vec3U(dest[0], dest[1], dest[2]) * dest[3] / 255;
      Vec3U srcMultiplied = Vec3U(src[0], src[1], src[2]) * src[3] / 255;

      // Src over dest alpha composition
      Vec3U over = srcMultiplied + destMultiplied * (255 - src[3]) / 255;
      unsigned alpha = src[3] + dest[3] * (255 - src[3]) / 255;

      auto assigned = set(x, y, Vec4B(over[0], over[1], over[2], alpha));
      if (!assigned)
        return std::unexpected(std::move(assigned.error()));
    }
  }
  return {};
}

Image Image::convert(PixelFormat pixelFormat) const {
  Image converted(m_width, m_height, pixelFormat);
  for (unsigned y = 0; y < m_height; ++y)
    for (unsigned x = 0; x < m_width; ++x) {
      auto pixel = bytesPerPixel() == 4 ? get32(x, y) : Vec4B(get24(x, y), 255);
      if (converted.bytesPerPixel() == 4)
        converted.set32(x, y, pixel);
      else
        converted.set24(x, y, pixel.vec3());
    }
  return converted;
}

std::expected<void, StarException> Image::writePng(Logger& logger, std::shared_ptr<IODevice> device) const {
  PngState state{logger, *device, true};
  auto initialized = initializePng(state);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  unsigned channels = m_pixelFormat == PixelFormat::RGB24 ? 3 : 4;
  auto rows = std::make_unique<png_bytep[]>(m_height);
  std::size_t stride = static_cast<std::size_t>(m_width) * channels;
  for (std::size_t i = 0; i < m_height; ++i)
    rows[i] = m_data + (m_height - i - 1) * stride;
  return state.check(writePngRows(state, m_width, m_height, channels, rows.get()), "Internal error writing png.");
}

ImageView::ImageView(Image const& image) {
  size = image.size();
  data = image.data();
  format = image.pixelFormat();
}

}

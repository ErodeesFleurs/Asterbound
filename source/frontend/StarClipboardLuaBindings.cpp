module;
#include <SDL3/SDL_surface.h>
#include <turbojpeg.h>
#include <webp/decode.h>
import std;
import star.json;
import star.format;
import star.lua;
import star.lua_converters;
import star.input_event;
import star.thread;
import star.listener;
import star.hash;
import star.io_device;
import star.string;
import star.buffer;
import star.ordered_map;
import star.bi_map;
import star.list;
import star.image_processing;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.input;
import star.asset_source;
import star.assets;
import star.root_base;
import star.logging;

module star.clipboard_lua_bindings;
import star.byte_array;
import star.vector;
import star.image_scaling;
namespace Star {
namespace {

constexpr unsigned maxPixels = 4 * 1024 * 1024;
constexpr unsigned thumbnailEdge = 128;

std::optional<Vec2U> thumbnail_size(unsigned width, unsigned height) {
  if (!width || !height || width > 4096 || height > 4096 || width * height > maxPixels)
    return {};
  auto longest = std::max(width, height);
  if (longest <= thumbnailEdge)
    return Vec2U(width, height);
  auto edge = [longest](unsigned extent) {
    return std::max(1u, (extent * thumbnailEdge + longest / 2) / longest);
  };
  return Vec2U(edge(width), edge(height));
}

std::optional<Image> decode_jpeg_thumbnail(std::string_view bytes) {
  std::unique_ptr<void, decltype(&tj3Destroy)> decoder(tj3Init(TJINIT_DECOMPRESS), tj3Destroy);
  auto handle = decoder.get();
  auto source = reinterpret_cast<unsigned char const*>(bytes.data());
  if (!handle || tj3Set(handle, TJPARAM_STOPONWARNING, 1) || tj3Set(handle, TJPARAM_SCANLIMIT, 100)
      || tj3Set(handle, TJPARAM_MAXPIXELS, maxPixels) || tj3Set(handle, TJPARAM_MAXMEMORY, 64)
      || tj3Set(handle, TJPARAM_SAVEMARKERS, 0) || tj3Set(handle, TJPARAM_BOTTOMUP, 1)
      || tj3DecompressHeader(handle, source, bytes.size()))
    return {};
  int width = tj3Get(handle, TJPARAM_JPEGWIDTH), height = tj3Get(handle, TJPARAM_JPEGHEIGHT);
  auto target = thumbnail_size(width, height);
  if (!target)
    return {};

  // Decode larger lossy images at a reduced IDCT size, then resample only the
  // residual difference. Round output dimensions from the original aspect ratio.
  auto longest = static_cast<unsigned>(std::max(width, height));
  tjscalingfactor scale{1, 1};
  if (!tj3Get(handle, TJPARAM_LOSSLESS)) {
    while (scale.denom < 8 && longest >= thumbnailEdge * scale.denom * 2)
      scale.denom *= 2;
    if (tj3SetScalingFactor(handle, scale))
      return {};
  }
  Vec2U decodedSize(static_cast<unsigned>(TJSCALED(width, scale)), static_cast<unsigned>(TJSCALED(height, scale)));
  // Image storage is bottom-up, matching the decoder's TJPARAM_BOTTOMUP.
  auto image = Image::for_overwrite(decodedSize, PixelFormat::RGB24);
  if (tj3Decompress8(handle, source, bytes.size(), image.span().data(), 0, TJPF_RGB))
    return {};
  if (decodedSize != *target) {
    auto thumb = scale_bilinear(image, Vec2F(*target).piecewise_divide(Vec2F(decodedSize)));
    if (!thumb)
      return {};
    return std::move(*thumb);
  }
  return image;
}

std::uint16_t le16(std::string_view bytes, size_t offset) {
  auto p = reinterpret_cast<unsigned char const*>(bytes.data() + offset);
  return static_cast<std::uint16_t>(p[0] | p[1] << 8);
}

std::uint32_t le32(std::string_view bytes, size_t offset) {
  return le16(bytes, offset) | (std::uint32_t(le16(bytes, offset + 2)) << 16);
}

std::optional<Image> decode_webp_thumbnail(std::string_view bytes) {
  if (le32(bytes, 4) != bytes.size() - 8)
    return {};
  auto data = reinterpret_cast<std::uint8_t const*>(bytes.data());
  WebPDecoderConfig config;
  if (!WebPInitDecoderConfig(&config)
      || WebPGetFeatures(data, bytes.size(), &config.input) != VP8_STATUS_OK || config.input.has_animation)
    return {};
  auto target = thumbnail_size(config.input.width, config.input.height);
  if (!target)
    return {};
  auto image = Image::for_overwrite(*target, config.input.has_alpha ? PixelFormat::RGBA32 : PixelFormat::RGB24);
  config.options.scaled_width = static_cast<int>(image.width());
  config.options.scaled_height = static_cast<int>(image.height());
  config.options.use_scaling = config.options.scaled_width != config.input.width || config.options.scaled_height != config.input.height;
  config.options.flip = 1; // libwebp normally produces top-down rows; Image is bottom-up.
  config.output.colorspace = config.input.has_alpha ? MODE_RGBA : MODE_RGB;
  config.output.is_external_memory = 1;
  config.output.u.RGBA = {image.span().data(), static_cast<int>(image.width() * image.bytes_per_pixel()), image.size_bytes()};
  auto status = WebPDecode(data, bytes.size(), &config);
  WebPFreeDecBuffer(&config.output);
  if (status != VP8_STATUS_OK)
    return {};
  return image;
}

std::optional<Image> decode_bmp_thumbnail(std::string_view bytes) {
  // Check the DIB dimensions and complete pixel array before SDL allocates its
  // surface. Accept CORE and Windows INFO-family headers with RGB/bitfields.
  if (bytes.size() < 26)
    return {};
  auto header = le32(bytes, 14);
  bool core = header == 12;
  if ((!core && header != 40 && header != 52 && header != 56 && header != 108 && header != 124)
      || header > bytes.size() - 14)
    return {};
  auto width = core ? le16(bytes, 18) : le32(bytes, 18);
  auto height = core ? le16(bytes, 20) : le32(bytes, 22);
  if (!core && (height & 0x80000000u))
    height = 0u - height; // Signed negative height denotes top-down rows.
  auto target = thumbnail_size(width, height);
  if (!target || le16(bytes, core ? 22 : 26) != 1)
    return {};
  auto bpp = le16(bytes, core ? 24 : 28);
  if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)
    return {};
  auto compression = core ? 0u : le32(bytes, 30);
  if (compression != 0 && !(compression == 3 && (bpp == 16 || bpp == 32)))
    return {};
  auto pixelOffset = le32(bytes, 10);
  auto fileSize = le32(bytes, 2);
  unsigned minOffset = 14 + header + (compression == 3 && header == 40 ? 12 : 0);
  if (bpp <= 8) {
    auto colors = core ? 0u : le32(bytes, 46);
    if (colors > (1u << bpp))
      return {};
    minOffset += (colors ? colors : (1u << bpp)) * (core ? 3 : 4);
  }
  auto stride = ((width * bpp + 31) / 32) * 4;
  if (fileSize > bytes.size() || pixelOffset < minOffset || pixelOffset > fileSize || stride * height > fileSize - pixelOffset)
    return {};

  using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
  Surface source(SDL_LoadBMP_IO(SDL_IOFromConstMem(bytes.data(), fileSize), true), SDL_DestroySurface);
  if (!source || !SDL_SetSurfaceBlendMode(source.get(), SDL_BLENDMODE_NONE))
    return {};
  auto image = Image::for_overwrite(*target, PixelFormat::RGBA32);
  Surface output(SDL_CreateSurfaceFrom(image.width(), image.height(), SDL_PIXELFORMAT_RGBA32,
      image.span().data(), image.width() * 4), SDL_DestroySurface);
  if (!output || !SDL_BlitSurfaceScaled(source.get(), nullptr, output.get(), nullptr, SDL_SCALEMODE_LINEAR)
      || !SDL_FlipSurface(output.get(), SDL_FLIP_VERTICAL))
    return {};
  return image;
}

std::optional<Image> decode_thumbnail(std::string_view bytes) {
  if (bytes.size() > 2 * 1024 * 1024)
    return {};
  if (bytes.starts_with("\xff\xd8\xff"))
    return decode_jpeg_thumbnail(bytes);
  if (bytes.starts_with("RIFF") && bytes.size() >= 12 && bytes.substr(8, 4) == "WEBP")
    return decode_webp_thumbnail(bytes);
  if (bytes.starts_with("BM"))
    return decode_bmp_thumbnail(bytes);
  return {};
}

}

std::expected<LuaCallbacks, StarException> LuaBindings::makeClipboardCallbacks(Input& input, RootBase& root, Logger& logger, std::shared_ptr<ApplicationController> appController, bool alwaysAllow) {
  LuaCallbacks callbacks;
  auto available = [appController, alwaysAllow, &input]() {
    return alwaysAllow || (appController->isFocused() && input.clipboard_allowed());
  };
  if (auto r = callbacks.register_callback("available", [=]() { return available(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("hasText", [=]() { return available() && appController->hasClipboard(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("getText", [=]() -> std::optional<String> {
    if (!available()) return {};
    return appController->getClipboard();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("getImage", [=]() -> std::optional<std::string> {
    if (!available()) return {};
    return appController->getClipboardImageBytes();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("imageToPng", [&logger](LuaEngine& engine, LuaString const& bytes) -> std::expected<LuaValue, StarException> {
    auto image = decode_thumbnail(bytes.view().utf8());
    if (!image) return lua_nil;
    auto buffer = std::make_shared<Buffer>();
    if (!image->write_png(logger, buffer)) return lua_nil;
    return engine.create_string(std::string_view(buffer->span().data(), buffer->data_size()));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("getFileReference", [=]() -> std::optional<std::string> {
    if (!available()) return {};
    return appController->getClipboardFileReference();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("setText", [=](String const& text) {
    return appController->isFocused() && appController->setClipboard(text);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("setData", [=](LuaTable const& data) -> std::expected<bool, StarException> {
    if (!appController->isFocused()) return false;
    StringMap<ByteArray> clipboardData;
    auto iteration = data.iterate([&](LuaValue const& key, LuaValue const& value) -> std::expected<void, StarException> {
      if (!std::holds_alternative<LuaString>(key) || !std::holds_alternative<LuaString>(value))
        return std::unexpected(LuaException("Clipboard data keys and values must be strings"));
      auto const& stringKey = std::get<LuaString>(key);
      auto const& stringValue = std::get<LuaString>(value);
      clipboardData[stringKey.to_string()] = ByteArray(std::span<char const>(stringValue.view().utf8()));
      return {};
    });
    if (!iteration) return std::unexpected(std::move(iteration.error()));
    return appController->setClipboardData(std::move(clipboardData));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("setImage", [appController, &root, &logger](LuaValue const& imgOrPath) -> std::expected<bool, StarException> {
    if (!appController->isFocused()) return false;
    auto buffer = std::make_shared<Buffer>();
    std::optional<std::reference_wrapper<Image const>> image;
    std::shared_ptr<Image const> ownedImage;
    if (std::holds_alternative<LuaUserData>(imgOrPath)) {
      auto const& userdata = std::get<LuaUserData>(imgOrPath);
      auto isImage = userdata.is<Image>();
      if (!isImage) return std::unexpected(std::move(isImage.error()));
      if (!*isImage) return std::unexpected(LuaException("Expected an Image or image asset path"));
      auto value = userdata.get<Image>();
      if (!value) return std::unexpected(std::move(value.error()));
      image = std::cref(value->get());
    } else {
      if (!std::holds_alternative<LuaString>(imgOrPath)) return std::unexpected(LuaException("Expected an Image or image asset path"));
      auto const& path = std::get<LuaString>(imgOrPath);
      auto assets = root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      auto loaded = (*assets)->image(path.to_string());
      if (!loaded) return std::unexpected(std::move(loaded.error()));
      ownedImage = std::move(*loaded);
      image = std::cref(*ownedImage);
    }
    if (auto written = image->get().write_png(logger, buffer); !written)
      return std::unexpected(std::move(written.error()));
    return appController->setClipboardImage(image->get(), std::ref(buffer->data()));
  }); !r) return std::unexpected(std::move(r.error()));
  return callbacks;
}

}

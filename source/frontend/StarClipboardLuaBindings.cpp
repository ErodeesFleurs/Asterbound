module;
#include <turbojpeg.h>
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

std::optional<Image> decode_jpeg_thumbnail(std::string_view bytes) {
  constexpr int maxPixels = 4 * 1024 * 1024;
  constexpr int thumbnailEdge = 128;
  if (bytes.empty() || bytes.size() > 2 * 1024 * 1024)
    return {};

  std::unique_ptr<void, decltype(&tj3Destroy)> decoder(tj3Init(TJINIT_DECOMPRESS), tj3Destroy);
  auto handle = decoder.get();
  auto source = reinterpret_cast<unsigned char const*>(bytes.data());
  if (!handle || tj3Set(handle, TJPARAM_STOPONWARNING, 1) || tj3Set(handle, TJPARAM_SCANLIMIT, 100)
      || tj3Set(handle, TJPARAM_MAXPIXELS, maxPixels) || tj3Set(handle, TJPARAM_MAXMEMORY, 64)
      || tj3Set(handle, TJPARAM_SAVEMARKERS, 0) || tj3Set(handle, TJPARAM_BOTTOMUP, 1)
      || tj3DecompressHeader(handle, source, bytes.size()))
    return {};
  int width = tj3Get(handle, TJPARAM_JPEGWIDTH), height = tj3Get(handle, TJPARAM_JPEGHEIGHT);
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || width * height > maxPixels)
    return {};

  // Decode larger lossy images at a reduced IDCT size, then resample only the
  // residual difference. Round output dimensions from the original aspect ratio.
  int longest = std::max(width, height);
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
  if (longest > thumbnailEdge) {
    auto edge = [longest](int extent) {
      return static_cast<unsigned>(std::max(1, (extent * thumbnailEdge + longest / 2) / longest));
    };
    Vec2U target(edge(width), edge(height));
    if (decodedSize != target) {
      auto thumb = scale_bilinear(image, Vec2F(target).piecewise_divide(Vec2F(decodedSize)));
      if (!thumb)
        return {};
      return std::move(*thumb);
    }
  }
  return image;
}

}

std::expected<LuaCallbacks, StarException> LuaBindings::makeClipboardCallbacks(Input& input, RootBase& root, Logger& logger, std::shared_ptr<ApplicationController> appController, bool alwaysAllow) {
  LuaCallbacks callbacks;
  auto available = [appController, alwaysAllow, &input]() {
    return alwaysAllow || (appController->isFocused() && input.clipboardAllowed());
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
    auto data = appController->getClipboardImageBytes(ApplicationController::ClipboardImageFormat::Png);
    if (data) return data;
    return appController->getClipboardImageBytes(ApplicationController::ClipboardImageFormat::Jpeg);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("jpegToPng", [&logger](LuaEngine& engine, LuaString const& bytes) -> std::expected<LuaValue, StarException> {
    auto image = decode_jpeg_thumbnail(bytes.view().utf8());
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

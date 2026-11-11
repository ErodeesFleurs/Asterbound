module;
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
namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeClipboardCallbacks(Input& input, RootBase& root, Logger& logger, std::shared_ptr<ApplicationController> appController, bool alwaysAllow) {
  LuaCallbacks callbacks;
  auto available = [appController, alwaysAllow, &input]() {
    return alwaysAllow || (appController->isFocused() && input.clipboardAllowed());
  };
  if (auto r = callbacks.registerCallback("available", [=]() { return available(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("hasText", [=]() { return available() && appController->hasClipboard(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("getText", [=]() -> std::optional<String> {
    if (!available()) return {};
    return appController->getClipboard();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setText", [=](String const& text) {
    return appController->isFocused() && appController->setClipboard(text);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setData", [=](LuaTable const& data) -> std::expected<bool, StarException> {
    if (!appController->isFocused()) return false;
    StringMap<ByteArray> clipboardData;
    auto iteration = data.iterate([&](LuaValue const& key, LuaValue const& value) -> std::expected<void, StarException> {
      if (!std::holds_alternative<LuaString>(key) || !std::holds_alternative<LuaString>(value))
        return std::unexpected(LuaException("Clipboard data keys and values must be strings"));
      auto const& stringKey = std::get<LuaString>(key);
      auto const& stringValue = std::get<LuaString>(value);
      clipboardData[stringKey.toString()] = ByteArray(std::span<char const>(stringValue.view().utf8()));
      return {};
    });
    if (!iteration) return std::unexpected(std::move(iteration.error()));
    return appController->setClipboardData(std::move(clipboardData));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setImage", [appController, &root, &logger](LuaValue const& imgOrPath) -> std::expected<bool, StarException> {
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
      auto loaded = (*assets)->image(path.toString());
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

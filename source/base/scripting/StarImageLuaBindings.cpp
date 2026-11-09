import std;
import star.format;
import star.string;
import star.json;
import star.json_extra;
import star.lua;
import star.lua_converters;
import star.image;
import star.color;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.optional;

import star.image_lua_bindings;

import star.vector;
namespace Star {

std::expected<LuaMethods<Image>, StarException> LuaUserDataMethods<Image>::make() {
  LuaMethods<Image> methods;

  if (auto result = methods.registerMethodWithSignature<Vec2U, Image&>("size", std::mem_fn(&Image::size)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = methods.registerMethod("drawInto", [](Image& image, Vec2U position, Image const& source) {
    return image.drawInto(position, source);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = methods.registerMethod("copyInto", [](Image& image, Vec2U position, Image const& source) {
    return image.copyInto(position, source);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = methods.registerMethod("set", [](Image& image, unsigned x, unsigned y, Color const& color) {
    return image.set(x, y, color.toRgba());
  }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = methods.registerMethod("get", [](Image& image, unsigned x, unsigned y) -> std::expected<Color, StarException> {
    auto pixel = image.get(x, y);
    if (!pixel)
      return std::unexpected(std::move(pixel.error()));
    return Color::rgba(*pixel);
  }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = methods.registerMethod("subImage", [](Image& image, Vec2U const& min, Vec2U const& size) {
    return image.subImage(min, size);
  }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = methods.registerMethod("process", [](Image& image, LuaEngine& engine, String const& directives) -> std::expected<Image, StarException> {
    List<std::shared_ptr<Image const>> resolvedImages;
    return processImageOperations(parseImageOperations(directives), image, [&](String const& path) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
      if (!engine.imageResolver())
        return std::nullopt;
      auto resolved = engine.imageResolver()(path);
      if (!resolved)
        return std::unexpected(std::move(resolved.error()));
      auto result = optionalRef(*resolved);
      resolvedImages.push_back(std::move(*resolved));
      return result;
    });
  }); !result)
    return std::unexpected(std::move(result.error()));

  return methods;
}

}

import std;
import star.format;
import star.string;
import star.json;
import star.json_extra;
import star.json_builder;
import star.json_parser;
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
import star.path_embed;
import star.buffer;
import star.data_stream_devices;
import star.logging;

import star.image_lua_bindings;

import star.vector;
namespace Star {

std::expected<LuaMethods<Image>, StarException> LuaUserDataMethods<Image>::make() {
  LuaMethods<Image> methods;

  if (auto result = methods.registerMethodWithSignature<Vec2U, Image&>("size", std::mem_fn(&Image::size)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = methods.registerMethod(
        "drawInto",
        [](Image& image, Vec2U position, Image const& source) { return image.draw_into(position, source); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = methods.registerMethod(
        "copyInto",
        [](Image& image, Vec2U position, Image const& source) { return image.copy_into(position, source); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = methods.registerMethod("set", [](Image& image, unsigned x, unsigned y, Color const& color) {
    return image.set(x, y, color.to_rgba());
  }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = methods.registerMethod("get", [](Image& image, unsigned x, unsigned y) -> std::expected<Color, StarException> {
    auto pixel = image.get(x, y);
    if (!pixel)
      return std::unexpected(std::move(pixel.error()));
    return Color::rgba(*pixel);
  }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = methods.registerMethod(
        "subImage", [](Image& image, Vec2U const& min, Vec2U const& size) { return image.sub_image(min, size); });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = methods.registerMethod("process", [](Image& image, LuaEngine& engine, String const& directives) -> std::expected<Image, StarException> {
    List<std::shared_ptr<Image const>> resolvedImages;
    return process_image_operations(
      parse_image_operations(directives), image,
      [&](String const& path) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
        if (!engine.imageResolver())
          return std::nullopt;
        auto resolved = engine.imageResolver()(path);
        if (!resolved)
          return std::unexpected(std::move(resolved.error()));
        auto result = optional_ref(*resolved);
        resolvedImages.push_back(std::move(*resolved));
        return result;
      });
  }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = methods.registerMethod("embed",
      [](Image const& image, String const& compression, std::optional<std::variant<String, Json>> const& frames,
          std::optional<String> const& framesCompression) -> std::expected<String, StarException> {
        auto method = embeddedCompressionMethod(compression);
        if (!method) return std::unexpected(std::move(method.error()));
        auto buffer = std::make_shared<Buffer>();
        Logger logger;
        if (auto written = image.write_png(logger, buffer); !written)
          return std::unexpected(std::move(written.error()));
        auto payload = embedAndCompressData(buffer->span(), *method);
        if (!payload) return std::unexpected(std::move(payload.error()));
        String output = String(frames ? std::string_view("/opensb_data;image;") : std::string_view("/opensb_data;bin;")) + *payload;
        if (!frames)
          return output;
        if (auto path = std::get_if<String>(&*frames)) {
          if (AssetPath::isEmbedded(*path)) {
            // A self-contained frames asset is represented inline, rather than
            // inserting its semicolons into the single framespath field.
            if (path->utf8().find_first_of(":?") != std::string::npos)
              return std::unexpected(StarException("Embedded frames cannot contain a subpath or directives"));
            output += String(";frames;") + String(std::string_view(path->utf8()).substr(std::string_view("/opensb_data;").size()));
          } else {
            if (path->empty() || path->utf8().find_first_of(";:?") != std::string::npos)
              return std::unexpected(StarException("Embedded frames path must be nonempty and contain no ';', ':' or '?'"));
            output += String(";framespath;") + *path;
          }
        } else {
          auto const& json = std::get<Json>(*frames);
          if (framesCompression && std::ranges::equal(framesCompression->utf8(), std::string_view("text"),
              [](char a, char b) { return (a >= 'A' && a <= 'Z' ? a + ('a' - 'A') : a) == b; })) {
            std::string text;
            JsonWriter<JsonUtf8StringOutputIterator, true, true> writer{JsonUtf8StringOutputIterator(text)};
            auto written = JsonStreamer<Json>::toJsonStream(json, writer, false);
            if (!written) return std::unexpected(std::move(written.error()));
            auto escaped = embedEscapeText(String(std::move(text)));
            if (!escaped) return std::unexpected(std::move(escaped.error()));
            output += String(";frames;text;") + *escaped;
          } else {
            auto framesMethod = *method;
            if (framesCompression) {
              auto parsed = embeddedCompressionMethod(*framesCompression);
              if (!parsed) return std::unexpected(std::move(parsed.error()));
              framesMethod = *parsed;
            }
            DataStreamBuffer stream;
            if (auto written = stream.write(json); !written)
              return std::unexpected(std::move(written.error()));
            auto encoded = embedAndCompressData(stream.span(), framesMethod);
            if (!encoded) return std::unexpected(std::move(encoded.error()));
            output += String(";frames;json;") + *encoded;
          }
        }
        return output;
      }); !result)
    return std::unexpected(std::move(result.error()));

  return methods;
}

}

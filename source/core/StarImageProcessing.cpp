import std;
import star.time;
import star.format;
import star.json;
import star.list;
import star.rect;
import star.image_processing;
import star.lexical_cast;
import star.color;
import star.image;
import star.string_view;
import star.string;
import star.byte_array;
import star.encode;
import star.logging;

import star.image_scaling;

import star.vector;
import star.math_common;
namespace Star {

std::expected<StringList, StarException> colorDirectivesFromConfig(JsonArray const& directives) {
  StringList result;
  for (auto const& entry : directives) {
    auto text = entry.type() == Json::Type::Object ? paletteSwapDirectivesFromConfig(entry) : entry.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    result.push_back(std::move(*text));
  }
  return result;
}

std::expected<String, StarException> paletteSwapDirectivesFromConfig(Json const& swaps) {
  ColorReplaceImageOperation paletteSwaps;
  auto object = swaps.iterateObject();
  if (!object)
    return std::unexpected(std::move(object.error()));
  for (auto const& swap : *object) {
    auto from = Color::fromHex(swap.first);
    if (!from)
      return std::unexpected(std::move(from.error()));
    auto text = swap.second.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto to = Color::fromHex(*text);
    if (!to)
      return std::unexpected(std::move(to.error()));
    paletteSwaps.colorReplaceMap[from->toRgba()] = to->toRgba();
  }
  return "?" + imageOperationToString(paletteSwaps);
}

HueShiftImageOperation HueShiftImageOperation::hueShiftDegrees(float degrees) {
  return HueShiftImageOperation{degrees / 360.0f};
}

SaturationShiftImageOperation SaturationShiftImageOperation::saturationShift100(float amount) {
  return SaturationShiftImageOperation{amount / 100.0f};
}

BrightnessMultiplyImageOperation BrightnessMultiplyImageOperation::brightnessMultiply100(float amount) {
  return BrightnessMultiplyImageOperation{amount / 100.0f + 1.0f};
}

FadeToColorImageOperation::FadeToColorImageOperation(Vec3B color, float amount) {
  this->color = color;
  this->amount = amount;

  auto fcl = Color::rgb(color).toLinear();
  for (int i = 0; i <= 255; ++i) {
    auto r = Color::rgb(Vec3B(i, i, i)).toLinear().mix(fcl, amount).toSRGB().toRgb();
    rTable[i] = r[0];
    gTable[i] = r[1];
    bTable[i] = r[2];
  }
}

ImageOperation imageOperationFromString(StringView string) {
  // Directives have an ASCII grammar; preserve non-ASCII reference bytes.
  std::string_view text = string.utf8();
  auto firstDelimiter = text.find_first_of("=;");
  if (text.substr(0, firstDelimiter) == "replace") {
    ColorReplaceImageOperation operation;
    if (firstDelimiter == std::string_view::npos)
      return operation;
    operation.colorReplaceMap.reserve((text.size() - firstDelimiter) / 8);
    std::optional<StringView> source;
    for (std::size_t begin = firstDelimiter + 1; begin < text.size();) {
      auto end = text.find_first_of("=;", begin);
      if (end == std::string_view::npos)
        end = text.size();
      if (end != begin) {
        StringView hex(text.substr(begin, end - begin));
        if (!source)
          source = hex;
        else {
          auto from = Color::hexToVec4B(*source);
          if (!from)
            return ErrorImageOperation{.cause = std::move(from.error())};
          auto to = Color::hexToVec4B(hex);
          if (!to)
            return ErrorImageOperation{.cause = std::move(to.error())};
          operation.colorReplaceMap[*from] = *to;
          source.reset();
        }
      }
      begin = end + 1;
    }
    return operation;
  }
  List<StringView> bits;
  for (std::size_t begin = 0; begin < text.size();) {
    auto end = text.find_first_of("=;", begin);
    if (end == std::string_view::npos)
      end = text.size();
    if (end != begin)
      bits.emplace_back(text.substr(begin, end - begin));
    begin = end + 1;
  }
  auto error = [](StarException cause) -> ImageOperation { return ErrorImageOperation{.cause = std::move(cause)}; };
  if (bits.empty())
    return error(ImageOperationException("Missing image operation"));
  auto type = bits[0];
  std::size_t required = 0;
  if (type == "hueshift" || type == "saturation" || type == "brightness" || type == "setcolor" || type == "multiply"
      || type == "addmask" || type == "submask" || type == "blendmult" || type == "blendscreen"
      || type == "scale" || type == "scalenearest" || type == "scalebilinear" || type == "scalebicubic")
    required = 1;
  else if (type == "fade" || type == "border" || type == "outline")
    required = 2;
  else if (type == "scanlines" || type == "crop")
    required = 4;
  if (bits.size() <= required)
    return error(ImageOperationException::format("Missing parameters for image operation '{}'", type));

  if (type == "flipx")
    return FlipImageOperation{FlipImageOperation::FlipX};
  if (type == "flipy")
    return FlipImageOperation{FlipImageOperation::FlipY};
  if (type == "flipxy")
    return FlipImageOperation{FlipImageOperation::FlipXY};
  if (type == "setcolor" || type == "multiply") {
    auto color = Color::fromHex(bits[1]);
    if (!color)
      return error(std::move(color.error()));
    if (type == "setcolor")
      return SetColorImageOperation{color->toRgb()};
    return MultiplyImageOperation{color->toRgba()};
  }
  if (type == "fade" || type == "scanlines") {
    auto color = Color::fromHex(bits[1]);
    if (!color)
      return error(std::move(color.error()));
    auto amount = lexicalCast<float>(bits[2]);
    if (!amount)
      return error(std::move(amount.error()));
    FadeToColorImageOperation fade(color->toRgb(), *amount);
    if (type == "fade")
      return fade;
    auto secondColor = Color::fromHex(bits[3]);
    if (!secondColor)
      return error(std::move(secondColor.error()));
    auto secondAmount = lexicalCast<float>(bits[4]);
    if (!secondAmount)
      return error(std::move(secondAmount.error()));
    return ScanLinesImageOperation{.fade1 = fade,
                                   .fade2 = FadeToColorImageOperation(secondColor->toRgb(), *secondAmount)};
  }
  if (type == "addmask" || type == "submask" || type == "blendmult" || type == "blendscreen") {
    StringList references;
    auto names = bits[1].utf8();
    for (std::size_t begin = 0; begin <= names.size();) {
      auto end = names.find('+', begin);
      if (end == std::string_view::npos)
        end = names.size();
      if (end != begin)
        references.emplace_back(names.substr(begin, end - begin));
      begin = end + 1;
    }
    Vec2I offset;
    for (std::size_t i = 2; i < std::min<std::size_t>(bits.size(), 4); ++i) {
      auto value = lexicalCast<int>(bits[i]);
      if (!value)
        return error(std::move(value.error()));
      offset[i - 2] = *value;
    }
    if (type == "addmask" || type == "submask")
      return AlphaMaskImageOperation{.mode = type == "addmask" ? AlphaMaskImageOperation::Additive
                                                               : AlphaMaskImageOperation::Subtractive,
                                     .maskImages = std::move(references),
                                     .offset = offset};
    return BlendImageOperation{.mode =
                                 type == "blendmult" ? BlendImageOperation::Multiply : BlendImageOperation::Screen,
                               .blendImages = std::move(references),
                               .offset = offset};
  }
  if (type == "border" || type == "outline") {
    auto pixels = lexicalCast<unsigned>(bits[1]);
    if (!pixels)
      return error(std::move(pixels.error()));
    auto start = Color::fromHex(bits[2]);
    if (!start)
      return error(std::move(start.error()));
    auto end = bits.size() > 3 ? Color::fromHex(bits[3]) : start;
    if (!end)
      return error(std::move(end.error()));
    return BorderImageOperation{.pixels = *pixels,
                                .startColor = start->toRgba(),
                                .endColor = end->toRgba(),
                                .outlineOnly = type == "outline",
                                .includeTransparent = false};
  }
  if (type == "crop") {
    float values[4]{};
    for (unsigned i = 0; i < 4; ++i) {
      auto value = lexicalCast<float>(bits[i + 1]);
      if (!value)
        return error(std::move(value.error()));
      values[i] = *value;
    }
    return CropImageOperation{RectI(values[0], values[1], values[2], values[3])};
  }
  if (type == "hueshift" || type == "saturation" || type == "brightness" || type == "scale"
      || type == "scalenearest" || type == "scalebilinear" || type == "scalebicubic") {
    auto value = lexicalCast<float>(bits[1]);
    if (!value)
      return error(std::move(value.error()));
    if (type == "hueshift")
      return HueShiftImageOperation::hueShiftDegrees(*value);
    if (type == "saturation")
      return SaturationShiftImageOperation::saturationShift100(*value);
    if (type == "brightness")
      return BrightnessMultiplyImageOperation::brightnessMultiply100(*value);
    auto second = bits.size() > 2 ? lexicalCast<float>(bits[2]) : value;
    if (!second)
      return error(std::move(second.error()));
    auto mode = type == "scalenearest" ? ScaleImageOperation::Nearest
      : type == "scalebicubic" ? ScaleImageOperation::Bicubic : ScaleImageOperation::Bilinear;
    return ScaleImageOperation{.mode = mode, .scale = Vec2F(*value, *second)};
  }
  return NullImageOperation{};
}

String imageOperationToString(ImageOperation const& operation) {
  if (auto op = (std::holds_alternative<HueShiftImageOperation>(operation) ? std::optional{std::cref(std::get<HueShiftImageOperation>(operation))} : std::nullopt)) {
    return strf("hueshift={}", op->get().hueShiftAmount * 360.0f);
  } else if (auto op = (std::holds_alternative<SaturationShiftImageOperation>(operation) ? std::optional{std::cref(std::get<SaturationShiftImageOperation>(operation))} : std::nullopt)) {
    return strf("saturation={}", op->get().saturationShiftAmount * 100.0f);
  } else if (auto op = (std::holds_alternative<BrightnessMultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<BrightnessMultiplyImageOperation>(operation))} : std::nullopt)) {
    return strf("brightness={}", (op->get().brightnessMultiply - 1.0f) * 100.0f);
  } else if (auto op = (std::holds_alternative<FadeToColorImageOperation>(operation) ? std::optional{std::cref(std::get<FadeToColorImageOperation>(operation))} : std::nullopt)) {
    return strf("fade={}={}", Color::rgb(op->get().color).toHex(), op->get().amount);
  } else if (auto op = (std::holds_alternative<ScanLinesImageOperation>(operation) ? std::optional{std::cref(std::get<ScanLinesImageOperation>(operation))} : std::nullopt)) {
    return strf("scanlines={}={}={}={}",
        Color::rgb(op->get().fade1.color).toHex(),
        op->get().fade1.amount,
        Color::rgb(op->get().fade2.color).toHex(),
        op->get().fade2.amount);
  } else if (auto op = (std::holds_alternative<SetColorImageOperation>(operation) ? std::optional{std::cref(std::get<SetColorImageOperation>(operation))} : std::nullopt)) {
    return strf("setcolor={}", Color::rgb(op->get().color).toHex());
  } else if (auto op = (std::holds_alternative<ColorReplaceImageOperation>(operation) ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))} : std::nullopt)) {
    String str = "replace";
    for (auto const& pair : op->get().colorReplaceMap)
      str += strf(";{}={}", Color::rgba(pair.first).toHex(), Color::rgba(pair.second).toHex());
    return str;
  } else if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == AlphaMaskImageOperation::Additive)
      return strf("addmask={};{};{}", op->get().maskImages.join("+"), op->get().offset[0], op->get().offset[1]);
    else if (op->get().mode == AlphaMaskImageOperation::Subtractive)
      return strf("submask={};{};{}", op->get().maskImages.join("+"), op->get().offset[0], op->get().offset[1]);
  } else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == BlendImageOperation::Multiply)
      return strf("blendmult={};{};{}", op->get().blendImages.join("+"), op->get().offset[0], op->get().offset[1]);
    else if (op->get().mode == BlendImageOperation::Screen)
      return strf("blendscreen={};{};{}", op->get().blendImages.join("+"), op->get().offset[0], op->get().offset[1]);
  } else if (auto op = (std::holds_alternative<MultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<MultiplyImageOperation>(operation))} : std::nullopt)) {
    return strf("multiply={}", Color::rgba(op->get().color).toHex());
  } else if (auto op = (std::holds_alternative<BorderImageOperation>(operation) ? std::optional{std::cref(std::get<BorderImageOperation>(operation))} : std::nullopt)) {
    if (op->get().outlineOnly)
      return strf("outline={};{};{}", op->get().pixels, Color::rgba(op->get().startColor).toHex(), Color::rgba(op->get().endColor).toHex());
    else
      return strf("border={};{};{}", op->get().pixels, Color::rgba(op->get().startColor).toHex(), Color::rgba(op->get().endColor).toHex());
  } else if (auto op = (std::holds_alternative<ScaleImageOperation>(operation) ? std::optional{std::cref(std::get<ScaleImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == ScaleImageOperation::Nearest)
      return strf("scalenearest={}", op->get().scale);
    else if (op->get().mode == ScaleImageOperation::Bilinear)
      return strf("scalebilinear={}", op->get().scale);
    else if (op->get().mode == ScaleImageOperation::Bicubic)
      return strf("scalebicubic={}", op->get().scale);
  } else if (auto op = (std::holds_alternative<CropImageOperation>(operation) ? std::optional{std::cref(std::get<CropImageOperation>(operation))} : std::nullopt)) {
    return strf("crop={};{};{};{}", op->get().subset.xMin(), op->get().subset.xMax(), op->get().subset.yMin(), op->get().subset.yMax());
  } else if (auto op = (std::holds_alternative<FlipImageOperation>(operation) ? std::optional{std::cref(std::get<FlipImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == FlipImageOperation::FlipX)
      return "flipx";
    else if (op->get().mode == FlipImageOperation::FlipY)
      return "flipy";
    else if (op->get().mode == FlipImageOperation::FlipXY)
      return "flipxy";
  }

  return "";
}

std::expected<void, StarException> parseImageOperations(StringView params, std::function<std::expected<void, StarException>(ImageOperation&&)> outputter) {
  if (!outputter)
    return std::unexpected(ImageOperationException("Missing image operation output callback"));
  auto text = params.utf8();
  for (std::size_t begin = 0; begin < text.size();) {
    auto end = text.find('?', begin);
    if (end == std::string_view::npos)
      end = text.size();
    if (end != begin) {
      auto result = outputter(imageOperationFromString(text.substr(begin, end - begin)));
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    begin = end + 1;
  }
  return {};
}

List<ImageOperation> parseImageOperations(StringView params) {
  List<ImageOperation> operations;
  auto text = params.utf8();
  for (std::size_t begin = 0; begin < text.size();) {
    auto end = text.find('?', begin);
    if (end == std::string_view::npos)
      end = text.size();
    if (end != begin)
      operations.push_back(imageOperationFromString(text.substr(begin, end - begin)));
    begin = end + 1;
  }
  return operations;
}

String printImageOperations(List<ImageOperation> const& list) {
  return StringList(list.transformed(imageOperationToString)).join("?");
}

void addImageOperationReferences(ImageOperation const& operation, StringList& out) {
  if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt))
    out.append_range(op->get().maskImages);
  else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt))
    out.append_range(op->get().blendImages);
}

StringList imageOperationReferences(List<ImageOperation> const& operations) {
  StringList references;
  for (auto const& operation : operations)
    addImageOperationReferences(operation, references);
  return references;
}

namespace {
// Keys are internal-format bytes, including alpha. Transparent HSV pixels never
// enter the cache, so zero is an unused key.
struct RecentColors {
  struct Entry {
    std::uint32_t in = 0;
    std::uint32_t out = 0;
  };

  static std::uint32_t pack(Vec4B const& pixel) {
    return std::uint32_t(pixel[0]) | std::uint32_t(pixel[1]) << 8 | std::uint32_t(pixel[2]) << 16
      | std::uint32_t(pixel[3]) << 24;
  }

  template <typename Function> void apply(Vec4B& pixel, Function&& compute) {
    auto key = pack(pixel);
    auto& entry = entries[(key * 2654435761u) >> 24];
    if (entry.in != key) {
      entry.out = pack(compute(pixel));
      entry.in = key;
    }
    auto value = entry.out;
    pixel = Vec4B(value & 255, (value >> 8) & 255, (value >> 16) & 255, value >> 24);
  }

  Entry entries[256];
};
}// namespace

static void processSaturationShift(Image& image, SaturationShiftImageOperation const& op) {
  RecentColors recent;
  image.forEachPixel([&](unsigned, unsigned, Vec4B& pixel) {
    if (pixel[3] != 0) {
      recent.apply(pixel, [&op](Vec4B const& input) {
        Color color = Color::rgba(input);
        color.setSaturation(std::clamp(color.saturation() + op.saturationShiftAmount, 0.0f, 1.0f));
        return color.toRgba();
      });
    }
  });
}

// Half-open pixel bounds. Widen before offset arithmetic: long is 32-bit on Windows.
struct PixelRect {
  std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  bool empty() const { return x0 >= x1 || y0 >= y1; }

  void include(PixelRect const& other) {
    if (other.empty())
      return;
    if (empty()) {
      *this = other;
      return;
    }
    x0 = std::min(x0, other.x0);
    y0 = std::min(y0, other.y0);
    x1 = std::max(x1, other.x1);
    y1 = std::max(y1, other.y1);
  }
};

// Four-byte images only: keep the map's internal channel order and one-step replacements.
static void replaceColors(ColorReplaceMap const& replaceMap, Image& image, PixelRect rect) {
  if (replaceMap.empty() || rect.empty())
    return;
  if (std::uint64_t(rect.x1 - rect.x0) * (rect.y1 - rect.y0) < 1024) {
    for (auto y = rect.y0; y < rect.y1; ++y) {
      for (auto x = rect.x0; x < rect.x1; ++x) {
        auto pixel = image.get32(x, y);
        if (auto replacement = replaceMap.maybe(pixel))
          image.set32(x, y, *replacement);
      }
    }
    return;
  }

  unsigned bitCount = 4096;
  while (replaceMap.size() > bitCount / 32 && bitCount < (1u << 20))
    bitCount <<= 1;
  unsigned shift = 32 - std::countr_zero(bitCount);
  std::array<std::uint64_t, 64> smallBits{};
  std::vector<std::uint64_t> largeBits;
  std::span<std::uint64_t> bits = smallBits;
  if (bitCount > 4096) {
    largeBits.resize(bitCount / 64);
    bits = largeBits;
  }
  auto bitFor = [shift](Vec4B const& pixel) { return (RecentColors::pack(pixel) * 2654435761u) >> shift; };
  for (auto const& entry : replaceMap) {
    auto bit = bitFor(entry.first);
    bits[bit >> 6] |= std::uint64_t(1) << (bit & 63);
  }

  static_assert(sizeof(Vec4B) == sizeof(std::uint32_t));
  auto data = image.data();
  std::uint32_t lastIn = 0, lastOut = 0;
  bool haveLast = false;
  for (auto y = rect.y0; y < rect.y1; ++y) {
    auto row = data.data() + (std::size_t(y) * image.width() + rect.x0) * 4;
    for (auto x = rect.x0; x < rect.x1; ++x) {
      auto destination = row + std::size_t(x - rect.x0) * 4;
      std::uint32_t input;
      std::memcpy(&input, destination, 4);
      if (haveLast && input == lastIn) {
        std::memcpy(destination, &lastOut, 4);
        continue;
      }
      Vec4B pixel;
      std::memcpy(pixel.data(), &input, 4);
      auto output = input;
      auto bit = bitFor(pixel);
      if ((bits[bit >> 6] >> (bit & 63)) & 1) {
        if (auto replacement = replaceMap.maybe(pixel)) {
          std::memcpy(&output, replacement->data(), 4);
          std::memcpy(destination, &output, 4);
        }
      }
      lastIn = input;
      lastOut = output;
      haveLast = true;
    }
  }
}

// Resolve only this operation's references, before changing any of its pixels.
static std::expected<PixelRect, StarException> applyBlend(BlendImageOperation const& op, Image& image,
                                                          ImageReferenceCallback const& refCallback) {
  if (op.blendImages.empty())
    return PixelRect{};
  if (!refCallback)
    return std::unexpected(ImageOperationException("Missing image ref callback during BlendImageOperation"));

  List<std::reference_wrapper<Image const>> blendImages;
  for (auto const& reference : op.blendImages) {
    auto blend = refCallback(reference);
    if (!blend)
      return std::unexpected(std::move(blend.error()));
    if (!*blend)
      return std::unexpected(ImageOperationException::format("Missing blend image '{}'", reference));
    blendImages.push_back(**blend);
  }

  std::int64_t offsetX = op.offset[0], offsetY = op.offset[1];
  PixelRect changed{.x0 = image.width(), .y0 = image.height(), .x1 = 0, .y1 = 0};
  for (auto blend : blendImages) {
    changed.x0 = std::min(changed.x0, std::max<std::int64_t>(0, -offsetX));
    changed.y0 = std::min(changed.y0, std::max<std::int64_t>(0, -offsetY));
    changed.x1 =
      std::max(changed.x1, std::min<std::int64_t>(image.width(), std::int64_t(blend.get().width()) - offsetX));
    changed.y1 =
      std::max(changed.y1, std::min<std::int64_t>(image.height(), std::int64_t(blend.get().height()) - offsetY));
  }
  auto blendPixel = [&](unsigned x, unsigned y, Vec4B& pixel) {
    std::int64_t px = std::int64_t(x) + offsetX, py = std::int64_t(y) + offsetY;
    Vec4F fpixel = Color::v4bToFloat(pixel);
    for (auto blend : blendImages) {
      if (px >= 0 && py >= 0 && px < blend.get().width() && py < blend.get().height()) {
        Vec4F other = Color::v4bToFloat(*blend.get().get(Vec2U(px, py)));
        if (op.mode == BlendImageOperation::Multiply)
          fpixel = fpixel.piecewiseMultiply(other);
        else if (op.mode == BlendImageOperation::Screen)
          fpixel = Vec4F::filled(1.0f) - (Vec4F::filled(1.0f) - fpixel).piecewiseMultiply(Vec4F::filled(1.0f) - other);
      }
    }
    pixel = Color::v4fToByte(fpixel);
  };
  if (image.bytesPerPixel() == 4) {
    for (auto y = changed.y0; y < changed.y1; ++y) {
      for (auto x = changed.x0; x < changed.x1; ++x) {
        auto pixel = image.get32(x, y);
        blendPixel(x, y, pixel);
        image.set32(x, y, pixel);
      }
    }
  } else {
    image.forEachPixel(blendPixel);
  }
  return changed;
}

std::expected<void, StarException> processImageOperation(ImageOperation const& operation, Image& image, ImageReferenceCallback refCallback) {
  if (auto error = (std::holds_alternative<ErrorImageOperation>(operation) ? std::optional{std::cref(std::get<ErrorImageOperation>(operation))} : std::nullopt))
    return std::unexpected(error->get().cause);
  if (image.bytesPerPixel() == 3) {
    // Convert to an image format that has alpha so certain operations function properly
    image = image.convert(image.pixelFormat() == PixelFormat::BGR24 ? PixelFormat::BGRA32 : PixelFormat::RGBA32);
  }
  if (auto op = (std::holds_alternative<HueShiftImageOperation>(operation) ? std::optional{std::cref(std::get<HueShiftImageOperation>(operation))} : std::nullopt)) {
    // Exact identities preserve the input bytes, rather than round-tripping HSV.
    // Keep these after the 24-to-32-bit conversion above.
    if (op->get().hueShiftAmount == 0.0f)
      return {};
    RecentColors recent;
    image.forEachPixel([&](unsigned, unsigned, Vec4B& pixel) {
      if (pixel[3] != 0)
        recent.apply(pixel, [&](Vec4B const& input) { return Color::hueShiftVec4B(input, op->get().hueShiftAmount); });
    });
  } else if (auto op = (std::holds_alternative<SaturationShiftImageOperation>(operation) ? std::optional{std::cref(std::get<SaturationShiftImageOperation>(operation))} : std::nullopt)) {
    if (op->get().saturationShiftAmount != 0.0f)
      processSaturationShift(image, op->get());
  } else if (auto op = (std::holds_alternative<BrightnessMultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<BrightnessMultiplyImageOperation>(operation))} : std::nullopt)) {
    if (op->get().brightnessMultiply == 1.0f)
      return {};
    RecentColors recent;
    image.forEachPixel([&](unsigned, unsigned, Vec4B& pixel) {
      if (pixel[3] != 0) {
        recent.apply(pixel, [&](Vec4B const& input) {
          Color color = Color::rgba(input);
          color.setValue(std::clamp(color.value() * op->get().brightnessMultiply, 0.0f, 1.0f));
          return color.toRgba();
        });
      }
    });
  } else if (auto op = (std::holds_alternative<FadeToColorImageOperation>(operation) ? std::optional{std::cref(std::get<FadeToColorImageOperation>(operation))} : std::nullopt)) {
    image.forEachPixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel[0] = op->get().rTable[pixel[0]];
      pixel[1] = op->get().gTable[pixel[1]];
      pixel[2] = op->get().bTable[pixel[2]];
    });
  } else if (auto op = (std::holds_alternative<ScanLinesImageOperation>(operation) ? std::optional{std::cref(std::get<ScanLinesImageOperation>(operation))} : std::nullopt)) {
    image.forEachPixel([&op](unsigned, unsigned y, Vec4B& pixel) {
      if (y % 2 == 0) {
        pixel[0] = op->get().fade1.rTable[pixel[0]];
        pixel[1] = op->get().fade1.gTable[pixel[1]];
        pixel[2] = op->get().fade1.bTable[pixel[2]];
      } else {
        pixel[0] = op->get().fade2.rTable[pixel[0]];
        pixel[1] = op->get().fade2.gTable[pixel[1]];
        pixel[2] = op->get().fade2.bTable[pixel[2]];
      }
    });
  } else if (auto op = (std::holds_alternative<SetColorImageOperation>(operation) ? std::optional{std::cref(std::get<SetColorImageOperation>(operation))} : std::nullopt)) {
    image.forEachPixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel[0] = op->get().color[0];
      pixel[1] = op->get().color[1];
      pixel[2] = op->get().color[2];
    });
  } else if (auto op = (std::holds_alternative<ColorReplaceImageOperation>(operation) ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))} : std::nullopt)) {
    auto const& replaceMap = op->get().colorReplaceMap;
    if (replaceMap.empty())
      return {};
    if (image.bytesPerPixel() == 4)
      replaceColors(replaceMap, image, {.x0 = 0, .y0 = 0, .x1 = image.width(), .y1 = image.height()});
    else
      image.forEachPixel([&](unsigned, unsigned, Vec4B& pixel) {
        if (auto replacement = replaceMap.maybe(pixel))
          pixel = *replacement;
      });

  } else if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt)) {
    if (op->get().maskImages.empty())
      return {};

    if (!refCallback)
      return std::unexpected(ImageOperationException("Missing image ref callback during AlphaMaskImageOperation"));

    List<std::reference_wrapper<Image const>> maskImages;
    for (auto const& reference : op->get().maskImages) {
      auto mask = refCallback(reference);
      if (!mask)
        return std::unexpected(std::move(mask.error()));
      if (!*mask)
        return std::unexpected(ImageOperationException::format("Missing mask image '{}'", reference));
      maskImages.push_back(**mask);
    }

    image.forEachPixel([&op, &maskImages](unsigned x, unsigned y, Vec4B& pixel) {
      std::uint8_t maskAlpha = 0;
      Vec2U pos = Vec2U(Vec2I(x, y) + op->get().offset);
      for (auto mask : maskImages) {
        if (pos[0] < mask.get().width() && pos[1] < mask.get().height()) {
          if (op->get().mode == AlphaMaskImageOperation::Additive) {
            // We produce our mask alpha from the maximum alpha of any of
            // the
            // mask images.
            maskAlpha = std::max(maskAlpha, mask.get().clamp(Vec2I(pos))[3]);
          } else if (op->get().mode == AlphaMaskImageOperation::Subtractive) {
            // We produce our mask alpha from the minimum alpha of any of
            // the
            // mask images.
            maskAlpha = std::min(maskAlpha, mask.get().clamp(Vec2I(pos))[3]);
          }
        }
      }
      pixel[3] = std::min(pixel[3], maskAlpha);
    });

  } else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt)) {
    auto blended = applyBlend(op->get(), image, refCallback);
    if (!blended)
      return std::unexpected(std::move(blended.error()));

  } else if (auto op = (std::holds_alternative<MultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<MultiplyImageOperation>(operation))} : std::nullopt)) {
    image.forEachPixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel = pixel.combine(op->get().color, [](std::uint8_t a, std::uint8_t b) -> std::uint8_t {
        return static_cast<std::uint8_t>((static_cast<int>(a) * static_cast<int>(b)) / 255);
      });
    });

  } else if (auto op = (std::holds_alternative<BorderImageOperation>(operation) ? std::optional{std::cref(std::get<BorderImageOperation>(operation))} : std::nullopt)) {
    Image borderImage(image.size() + Vec2U::filled(op->get().pixels * 2), PixelFormat::RGBA32);
    auto copied = borderImage.copyInto(Vec2U::filled(op->get().pixels), image);
    if (!copied)
      return std::unexpected(std::move(copied.error()));
    Vec2I borderImageSize = Vec2I(borderImage.size());

    borderImage.forEachPixel([&op, &image, &borderImageSize](int x, int y, Vec4B& pixel) {
      int pixels = op->get().pixels;
      bool includeTransparent = op->get().includeTransparent;
      if (pixel[3] == 0 || (includeTransparent && pixel[3] != 255)) {
        int dist = std::numeric_limits<int>::max();
        for (int j = -pixels; j < pixels + 1; j++) {
          for (int i = -pixels; i < pixels + 1; i++) {
            if (i + x >= pixels && j + y >= pixels && i + x < borderImageSize[0] - pixels && j + y < borderImageSize[1] - pixels) {
              Vec4B remotePixel = image.get32(i + x - pixels, j + y - pixels);
              if (remotePixel[3] != 0) {
                dist = std::min(dist, std::abs(i) + std::abs(j));
                if (dist == 1) // Early out, if dist is 1 it ain't getting shorter
                  break;
              }
            }
          }
        }

        if (dist < std::numeric_limits<int>::max()) {
          float percent = (dist - 1) / (2.0f * pixels - 1);
          if (pixel[3] != 0) {
            Color color = Color::rgba(op->get().startColor).mix(Color::rgba(op->get().endColor), percent);
            if (op->get().outlineOnly) {
              float pixelA = byteToFloat(pixel[3]);
              color.setAlphaF((1.0f - pixelA) * std::fminf(pixelA, 0.5f) * 2.0f);
            }
            else {
              Color pixelF = Color::rgba(pixel);
              float pixelA = pixelF.alphaF(), colorA = color.alphaF();
              colorA += pixelA * (1.0f - colorA);
              pixelF.convertToLinear(); //Mix in linear color space as it is more perceptually accurate
              color.convertToLinear();
              color = color.mix(pixelF, pixelA);
              color.convertToSRGB();
              color.setAlphaF(colorA);
            }
            pixel = color.toRgba();
          } else {
            pixel = Vec4B(Vec4F(op->get().startColor) * (1 - percent) + Vec4F(op->get().endColor) * percent);
          }
        }
      } else if (op->get().outlineOnly) {
        pixel = Vec4B(0, 0, 0, 0);
      }
    });

    image = borderImage;

  } else if (auto op = (std::holds_alternative<ScaleImageOperation>(operation) ? std::optional{std::cref(std::get<ScaleImageOperation>(operation))} : std::nullopt)) {
    auto scale = op->get().scale;
    if (scale[0] < 0.0f || scale[1] < 0.0f) {
      EmergencyLog::warn("Negative scale in ScaleImageOperation ({})", scale);
      scale = scale.piecewiseMax(Vec2F::filled(0.f));
    }
    auto scaled = op->get().mode == ScaleImageOperation::Nearest ? scaleNearest(image, scale)
      : op->get().mode == ScaleImageOperation::Bicubic ? scaleBicubic(image, scale) : scaleBilinear(image, scale);
    if (!scaled)
      return std::unexpected(std::move(scaled.error()));
    image = std::move(*scaled);

  } else if (auto op = (std::holds_alternative<CropImageOperation>(operation) ? std::optional{std::cref(std::get<CropImageOperation>(operation))} : std::nullopt)) {
    auto cropped = image.subImage(Vec2U(op->get().subset.min()), Vec2U(op->get().subset.size()));
    if (!cropped)
      return std::unexpected(std::move(cropped.error()));
    image = std::move(*cropped);

  } else if (auto op = (std::holds_alternative<FlipImageOperation>(operation) ? std::optional{std::cref(std::get<FlipImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == FlipImageOperation::FlipX || op->get().mode == FlipImageOperation::FlipXY) {
      for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t xLeft = 0; xLeft < image.width() / 2; ++xLeft) {
          std::size_t xRight = image.width() - 1 - xLeft;

          auto left = image.get32(xLeft, y);
          auto right = image.get32(xRight, y);

          image.set32(xLeft, y, right);
          image.set32(xRight, y, left);
        }
      }
    }

    if (op->get().mode == FlipImageOperation::FlipY || op->get().mode == FlipImageOperation::FlipXY) {
      for (std::size_t x = 0; x < image.width(); ++x) {
        for (std::size_t yTop = 0; yTop < image.height() / 2; ++yTop) {
          std::size_t yBottom = image.height() - 1 - yTop;

          auto top = image.get32(x, yTop);
          auto bottom = image.get32(x, yBottom);

          image.set32(x, yTop, bottom);
          image.set32(x, yBottom, top);
        }
      }
    }
  }
  return {};
}

static bool isPerPixelColorOperation(ImageOperation const* operation) {
  return operation
    && (std::holds_alternative<HueShiftImageOperation>(*operation)
        || std::holds_alternative<SaturationShiftImageOperation>(*operation)
        || std::holds_alternative<BrightnessMultiplyImageOperation>(*operation)
        || std::holds_alternative<FadeToColorImageOperation>(*operation)
        || std::holds_alternative<SetColorImageOperation>(*operation)
        || std::holds_alternative<ColorReplaceImageOperation>(*operation)
        || std::holds_alternative<MultiplyImageOperation>(*operation));
}

static bool hasBytePixels(Image const& image) { return image.bytesPerPixel() == 3 || image.bytesPerPixel() == 4; }

static bool scaleEnlargesByCopying(ScaleImageOperation const& scale, Image const& image) {
  // Empty scales synthesize black pixels, rather than copying source pixels.
  // Float images also have different scalar byte-setter semantics.
  if (image.empty() || !hasBytePixels(image) || scale.mode != ScaleImageOperation::Nearest
      || !scaledCropFits(image, scale.scale, RectI(0, 0, 0, 0)))
    return false;
  auto size = Vec2U::round(vmult(Vec2F(image.size()), scale.scale));
  size[0] = std::max(size[0], 1u);
  size[1] = std::max(size[1], 1u);
  return std::uint64_t(size[0]) * size[1] > std::uint64_t(image.width()) * image.height();
}

static bool cropShrinks(CropImageOperation const& crop, Image const& image) {
  auto min = crop.subset.min(), max = crop.subset.max();
  if (!hasBytePixels(image) || min[0] < 0 || min[1] < 0 || max[0] < min[0] || max[1] < min[1]
      || std::uint64_t(max[0]) > image.width() || std::uint64_t(max[1]) > image.height())
    return false;
  return std::uint64_t(max[0] - min[0]) * (max[1] - min[1]) < std::uint64_t(image.width()) * image.height();
}

static std::expected<bool, StarException> processScaleCrop(ScaleImageOperation const& scale,
                                                           CropImageOperation const& crop, Image& image) {
  // Leave invalid crops/scales and float formats to the sequential error path.
  if (!hasBytePixels(image) || scale.mode == ScaleImageOperation::Bicubic
      || !scaledCropFits(image, scale.scale, crop.subset))
    return false;
  if (image.bytesPerPixel() == 3)
    image = image.convert(image.pixelFormat() == PixelFormat::BGR24 ? PixelFormat::BGRA32 : PixelFormat::RGBA32);
  auto scaled = scale.mode == ScaleImageOperation::Nearest ? scaleNearestCropped(image, scale.scale, crop.subset)
                                                           : scaleBilinearCropped(image, scale.scale, crop.subset);
  if (!scaled)
    return std::unexpected(std::move(scaled.error()));
  image = std::move(*scaled);
  return true;
}

static bool replaceSettles(ColorReplaceMap const& map) {
  for (auto const& entry : map) {
    if (map.contains(entry.second))
      return false;
  }
  return true;
}

static bool replaceKeysAreKeysOf(ColorReplaceMap const& map, ColorReplaceMap const& other) {
  for (auto const& entry : map) {
    if (!other.contains(entry.first))
      return false;
  }
  return true;
}

// An accessor keeps the contiguous List entrypoint allocation-free, while directives
// can refer to stable, already loaded operations without copying their maps/tables.
template <typename OperationAt>
static std::expected<void, StarException> processImageOperationSequence(std::size_t count, OperationAt operationAt,
                                                                        Image& image,
                                                                        ImageReferenceCallback const& refCallback) {
  ColorReplaceImageOperation const* settled = nullptr;
  PixelRect changed;
  bool sawBlend = false;
  for (std::size_t i = 0; i < count;) {
    auto operation = operationAt(i);
    if (!operation)
      return std::unexpected(ImageOperationException("Null image operation pointer"));

    if (auto blend = std::get_if<BlendImageOperation>(operation); blend && settled) {
      // Match processImageOperation's per-operation callback value semantics.
      auto operationCallback = refCallback;
      auto blended = applyBlend(*blend, image, operationCallback);
      if (!blended)
        return std::unexpected(std::move(blended.error()));
      changed.include(*blended);
      sawBlend = true;
      ++i;
      continue;
    }
    if (auto replace = std::get_if<ColorReplaceImageOperation>(operation); replace && settled && sawBlend
        && replaceKeysAreKeysOf(replace->colorReplaceMap, settled->colorReplaceMap)
        && replaceSettles(replace->colorReplaceMap)) {
      // Outside the consecutive blends' union there are no keys left to replace.
      replaceColors(replace->colorReplaceMap, image, changed);
      settled = replace;
      changed = {};
      sawBlend = false;
      ++i;
      continue;
    }
    settled = nullptr;
    changed = {};
    sawBlend = false;

    if (isPerPixelColorOperation(operation)) {
      auto end = i + 1;
      while (end < count && isPerPixelColorOperation(operationAt(end)))
        ++end;
      if (end < count && operationAt(end)) {
        if (auto crop = std::get_if<CropImageOperation>(operationAt(end)); crop && cropShrinks(*crop, image)) {
          auto cropped = processImageOperation(*operationAt(end), image, refCallback);
          if (!cropped)
            return std::unexpected(std::move(cropped.error()));
          for (auto k = i; k < end; ++k) {
            auto processed = processImageOperation(*operationAt(k), image, refCallback);
            if (!processed)
              return std::unexpected(std::move(processed.error()));
          }
          i = end + 1;
          continue;
        }
      }
      for (auto k = i; k < end; ++k) {
        auto processed = processImageOperation(*operationAt(k), image, refCallback);
        if (!processed)
          return std::unexpected(std::move(processed.error()));
      }
      // Only a successfully applied final replace establishes the settled invariant.
      if (image.bytesPerPixel() == 4 && end < count && operationAt(end)
          && std::holds_alternative<BlendImageOperation>(*operationAt(end))) {
        auto replace = std::get_if<ColorReplaceImageOperation>(operationAt(end - 1));
        if (replace && replaceSettles(replace->colorReplaceMap))
          settled = replace;
      }
      i = end;
      continue;
    }

    if (auto scale = std::get_if<ScaleImageOperation>(operation)) {
      auto end = i + 1;
      while (end < count && isPerPixelColorOperation(operationAt(end)))
        ++end;
      if (end > i + 1 && scaleEnlargesByCopying(*scale, image)) {
        for (auto k = i + 1; k < end; ++k) {
          auto processed = processImageOperation(*operationAt(k), image, refCallback);
          if (!processed)
            return std::unexpected(std::move(processed.error()));
        }
        if (end < count && operationAt(end)) {
          if (auto crop = std::get_if<CropImageOperation>(operationAt(end))) {
            auto fused = processScaleCrop(*scale, *crop, image);
            if (!fused)
              return std::unexpected(std::move(fused.error()));
            if (*fused) {
              i = end + 1;
              continue;
            }
          }
        }
        auto scaled = processImageOperation(*operation, image, refCallback);
        if (!scaled)
          return std::unexpected(std::move(scaled.error()));
        i = end;
        continue;
      }
      if (i + 1 < count && operationAt(i + 1)) {
        if (auto crop = std::get_if<CropImageOperation>(operationAt(i + 1))) {
          auto fused = processScaleCrop(*scale, *crop, image);
          if (!fused)
            return std::unexpected(std::move(fused.error()));
          if (*fused) {
            i += 2;
            continue;
          }
        }
      }
    }
    auto processed = processImageOperation(*operation, image, refCallback);
    if (!processed)
      return std::unexpected(std::move(processed.error()));
    ++i;
  }
  return {};
}

std::expected<void, StarException> processImageOperations(std::span<ImageOperation const* const> operations,
                                                          Image& image, ImageReferenceCallback refCallback) {
  return processImageOperationSequence(
    operations.size(), [&](std::size_t i) { return operations[i]; }, image, refCallback);
}

std::expected<Image, StarException> processImageOperations(List<ImageOperation> const& operations, Image image,
                                                           ImageReferenceCallback refCallback) {
  auto processed =
    processImageOperationSequence(operations.size(), [&](std::size_t i) { return &operations[i]; }, image, refCallback);
  if (!processed)
    return std::unexpected(std::move(processed.error()));
  return image;
}
}

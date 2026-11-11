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

std::expected<StringList, StarException> color_directives_from_config(JsonArray const& directives) {
  StringList result;
  for (auto const& entry : directives) {
    auto text = entry.type() == Json::Type::Object ? palette_swap_directives_from_config(entry) : entry.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    result.push_back(std::move(*text));
  }
  return result;
}

std::expected<String, StarException> palette_swap_directives_from_config(Json const& swaps) {
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
  return "?" + image_operation_to_string(paletteSwaps);
}

HueShiftImageOperation HueShiftImageOperation::hue_shift_degrees(float degrees) {
  return HueShiftImageOperation{degrees / 360.0f};
}

SaturationShiftImageOperation SaturationShiftImageOperation::saturation_shift_100(float amount) {
  return SaturationShiftImageOperation{amount / 100.0f};
}

BrightnessMultiplyImageOperation BrightnessMultiplyImageOperation::brightness_multiply_100(float amount) {
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

ImageOperation image_operation_from_string(StringView string) {
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
    auto amount = lexical_cast<float>(bits[2]);
    if (!amount)
      return error(std::move(amount.error()));
    FadeToColorImageOperation fade(color->toRgb(), *amount);
    if (type == "fade")
      return fade;
    auto secondColor = Color::fromHex(bits[3]);
    if (!secondColor)
      return error(std::move(secondColor.error()));
    auto secondAmount = lexical_cast<float>(bits[4]);
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
      auto value = lexical_cast<int>(bits[i]);
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
    auto pixels = lexical_cast<unsigned>(bits[1]);
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
      auto value = lexical_cast<float>(bits[i + 1]);
      if (!value)
        return error(std::move(value.error()));
      values[i] = *value;
    }
    return CropImageOperation{RectI(values[0], values[1], values[2], values[3])};
  }
  if (type == "hueshift" || type == "saturation" || type == "brightness" || type == "scale"
      || type == "scalenearest" || type == "scalebilinear" || type == "scalebicubic") {
    auto value = lexical_cast<float>(bits[1]);
    if (!value)
      return error(std::move(value.error()));
    if (type == "hueshift")
      return HueShiftImageOperation::hue_shift_degrees(*value);
    if (type == "saturation")
      return SaturationShiftImageOperation::saturation_shift_100(*value);
    if (type == "brightness")
      return BrightnessMultiplyImageOperation::brightness_multiply_100(*value);
    auto second = bits.size() > 2 ? lexical_cast<float>(bits[2]) : value;
    if (!second)
      return error(std::move(second.error()));
    auto mode = type == "scalenearest" ? ScaleImageOperation::Nearest
      : type == "scalebicubic" ? ScaleImageOperation::Bicubic : ScaleImageOperation::Bilinear;
    return ScaleImageOperation{.mode = mode, .scale = Vec2F(*value, *second)};
  }
  return NullImageOperation{};
}

String image_operation_to_string(ImageOperation const& operation) {
  if (auto op = (std::holds_alternative<HueShiftImageOperation>(operation) ? std::optional{std::cref(std::get<HueShiftImageOperation>(operation))} : std::nullopt)) {
    return Star::format("hueshift={}", op->get().hueShiftAmount * 360.0f);
  } else if (auto op = (std::holds_alternative<SaturationShiftImageOperation>(operation) ? std::optional{std::cref(std::get<SaturationShiftImageOperation>(operation))} : std::nullopt)) {
    return Star::format("saturation={}", op->get().saturationShiftAmount * 100.0f);
  } else if (auto op = (std::holds_alternative<BrightnessMultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<BrightnessMultiplyImageOperation>(operation))} : std::nullopt)) {
    return Star::format("brightness={}", (op->get().brightnessMultiply - 1.0f) * 100.0f);
  } else if (auto op = (std::holds_alternative<FadeToColorImageOperation>(operation) ? std::optional{std::cref(std::get<FadeToColorImageOperation>(operation))} : std::nullopt)) {
    return Star::format("fade={}={}", Color::rgb(op->get().color).toHex(), op->get().amount);
  } else if (auto op = (std::holds_alternative<ScanLinesImageOperation>(operation) ? std::optional{std::cref(std::get<ScanLinesImageOperation>(operation))} : std::nullopt)) {
    return Star::format("scanlines={}={}={}={}",
        Color::rgb(op->get().fade1.color).toHex(),
        op->get().fade1.amount,
        Color::rgb(op->get().fade2.color).toHex(),
        op->get().fade2.amount);
  } else if (auto op = (std::holds_alternative<SetColorImageOperation>(operation) ? std::optional{std::cref(std::get<SetColorImageOperation>(operation))} : std::nullopt)) {
    return Star::format("setcolor={}", Color::rgb(op->get().color).toHex());
  } else if (auto op = (std::holds_alternative<ColorReplaceImageOperation>(operation) ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))} : std::nullopt)) {
    String str = "replace";
    for (auto const& pair : op->get().colorReplaceMap)
      str += Star::format(";{}={}", Color::rgba(pair.first).toHex(), Color::rgba(pair.second).toHex());
    return str;
  } else if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == AlphaMaskImageOperation::Additive)
      return Star::format("addmask={};{};{}", op->get().maskImages.join("+"), op->get().offset[0], op->get().offset[1]);
    else if (op->get().mode == AlphaMaskImageOperation::Subtractive)
      return Star::format("submask={};{};{}", op->get().maskImages.join("+"), op->get().offset[0], op->get().offset[1]);
  } else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == BlendImageOperation::Multiply)
      return Star::format("blendmult={};{};{}", op->get().blendImages.join("+"), op->get().offset[0], op->get().offset[1]);
    else if (op->get().mode == BlendImageOperation::Screen)
      return Star::format("blendscreen={};{};{}", op->get().blendImages.join("+"), op->get().offset[0], op->get().offset[1]);
  } else if (auto op = (std::holds_alternative<MultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<MultiplyImageOperation>(operation))} : std::nullopt)) {
    return Star::format("multiply={}", Color::rgba(op->get().color).toHex());
  } else if (auto op = (std::holds_alternative<BorderImageOperation>(operation) ? std::optional{std::cref(std::get<BorderImageOperation>(operation))} : std::nullopt)) {
    if (op->get().outlineOnly)
      return Star::format("outline={};{};{}", op->get().pixels, Color::rgba(op->get().startColor).toHex(), Color::rgba(op->get().endColor).toHex());
    else
      return Star::format("border={};{};{}", op->get().pixels, Color::rgba(op->get().startColor).toHex(), Color::rgba(op->get().endColor).toHex());
  } else if (auto op = (std::holds_alternative<ScaleImageOperation>(operation) ? std::optional{std::cref(std::get<ScaleImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == ScaleImageOperation::Nearest)
      return Star::format("scalenearest={}", op->get().scale);
    else if (op->get().mode == ScaleImageOperation::Bilinear)
      return Star::format("scalebilinear={}", op->get().scale);
    else if (op->get().mode == ScaleImageOperation::Bicubic)
      return Star::format("scalebicubic={}", op->get().scale);
  } else if (auto op = (std::holds_alternative<CropImageOperation>(operation) ? std::optional{std::cref(std::get<CropImageOperation>(operation))} : std::nullopt)) {
    return Star::format("crop={};{};{};{}", op->get().subset.xMin(), op->get().subset.xMax(), op->get().subset.yMin(), op->get().subset.yMax());
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

std::expected<void, StarException>
parse_image_operations(StringView params,
                       std::function<std::expected<void, StarException>(ImageOperation&&)> outputter) {
  if (!outputter)
    return std::unexpected(ImageOperationException("Missing image operation output callback"));
  auto text = params.utf8();
  for (std::size_t begin = 0; begin < text.size();) {
    auto end = text.find('?', begin);
    if (end == std::string_view::npos)
      end = text.size();
    if (end != begin) {
      auto result = outputter(image_operation_from_string(text.substr(begin, end - begin)));
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    begin = end + 1;
  }
  return {};
}

List<ImageOperation> parse_image_operations(StringView params) {
  List<ImageOperation> operations;
  auto text = params.utf8();
  for (std::size_t begin = 0; begin < text.size();) {
    auto end = text.find('?', begin);
    if (end == std::string_view::npos)
      end = text.size();
    if (end != begin)
      operations.push_back(image_operation_from_string(text.substr(begin, end - begin)));
    begin = end + 1;
  }
  return operations;
}

String print_image_operations(List<ImageOperation> const& list) {
  return StringList(list.transformed(image_operation_to_string)).join("?");
}

void add_image_operation_references(ImageOperation const& operation, StringList& out) {
  if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt))
    out.append_range(op->get().maskImages);
  else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt))
    out.append_range(op->get().blendImages);
}

StringList image_operation_references(List<ImageOperation> const& operations) {
  StringList references;
  for (auto const& operation : operations)
    add_image_operation_references(operation, references);
  return references;
}

namespace {
template <typename Function>
concept pixel_color_transform = requires(Function& function, Vec4B const& pixel) { Vec4B(function(pixel)); };

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

  template <pixel_color_transform Function> void apply(Vec4B& pixel, Function&& compute) {
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

static void process_saturation_shift(Image& image, SaturationShiftImageOperation const& op) {
  RecentColors recent;
  image.for_each_pixel([&](unsigned, unsigned, Vec4B& pixel) {
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
static void replace_colors(ColorReplaceMap const& replaceMap, Image& image, PixelRect rect) {
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
    auto row = data.subspan((std::size_t(y) * image.width() + rect.x0) * 4, std::size_t(rect.x1 - rect.x0) * 4);
    auto destination = row.begin();
    for (auto x = rect.x0; x < rect.x1; ++x, std::advance(destination, 4)) {
      std::array<std::uint8_t, 4> inputBytes;
      std::ranges::copy_n(destination, 4, inputBytes.begin());
      auto input = std::bit_cast<std::uint32_t>(inputBytes);
      if (haveLast && input == lastIn) {
        std::ranges::copy(std::bit_cast<std::array<std::uint8_t, 4>>(lastOut), destination);
        continue;
      }
      Vec4B pixel;
      std::ranges::copy(inputBytes, pixel.begin());
      auto output = input;
      auto bit = bitFor(pixel);
      if ((bits[bit >> 6] >> (bit & 63)) & 1) {
        if (auto replacement = replaceMap.maybe(pixel)) {
          std::ranges::copy(*replacement, inputBytes.begin());
          output = std::bit_cast<std::uint32_t>(inputBytes);
          std::ranges::copy(inputBytes, destination);
        }
      }
      lastIn = input;
      lastOut = output;
      haveLast = true;
    }
  }
}

// Built after all reference callbacks: callbacks may change a previously returned
// image. Iterators still observe target aliases and each pixel's final commit.
struct PreparedImageReference {
  std::reference_wrapper<Image const> image;
  std::span<std::uint8_t const>::iterator data;
  unsigned width = 0, height = 0, channels = 0;
  std::size_t stride = 0;

  explicit PreparedImageReference(Image const& source) : image(source) {}

  void prepare() {
    data = image.get().data().begin();
    width = image.get().width();
    height = image.get().height();
    channels = image.get().bytes_per_pixel();
    stride = std::size_t(width) * channels;
  }

  Vec4B read(unsigned x, unsigned y) const {
    if (channels == 4) {
      Vec4B pixel;
      std::ranges::copy_n(std::next(data, std::size_t(y) * stride + std::size_t(x) * 4), 4, pixel.begin());
      return pixel;
    }
    if (channels == 3) {
      auto pixel = std::next(data, std::size_t(y) * stride + std::size_t(x) * 3);
      return Vec4B(pixel[0], pixel[1], pixel[2], 255);
    }
    return *image.get().get(Vec2U(x, y));
  }

  std::uint8_t alpha(Vec2U position) const {
    // Match clamp's signed-coordinate behavior, even for unsigned positions.
    auto x = std::max(0, int(position[0])), y = std::max(0, int(position[1]));
    if (channels == 4)
      return *std::next(data, std::size_t(y) * stride + std::size_t(x) * 4 + 3);
    if (channels == 3)
      return 255;
    return image.get().clamp(Vec2I(position))[3];
  }
};

template <int Mode>
concept blend_kernel_mode = Mode == BlendImageOperation::Multiply || Mode == BlendImageOperation::Screen || Mode == -1;

// Resolve only this operation's references, before changing any of its pixels.
static std::expected<PixelRect, StarException> apply_blend(BlendImageOperation const& op, Image& image,
                                                           ImageReferenceCallback const& refCallback) {
  if (op.blendImages.empty())
    return PixelRect{};
  if (!refCallback)
    return std::unexpected(ImageOperationException("Missing image ref callback during BlendImageOperation"));

  List<PreparedImageReference> sources;
  sources.reserve(op.blendImages.size());
  for (auto const& reference : op.blendImages) {
    auto blend = refCallback(reference);
    if (!blend)
      return std::unexpected(std::move(blend.error()));
    if (!*blend)
      return std::unexpected(ImageOperationException::format("Missing blend image '{}'", reference));
    sources.emplace_back((**blend).get());
  }

  for (auto& source : sources)
    source.prepare();

  std::int64_t offsetX = op.offset[0], offsetY = op.offset[1];
  PixelRect changed{.x0 = image.width(), .y0 = image.height(), .x1 = 0, .y1 = 0};
  for (auto const& blend : sources) {
    changed.x0 = std::min(changed.x0, std::max<std::int64_t>(0, -offsetX));
    changed.y0 = std::min(changed.y0, std::max<std::int64_t>(0, -offsetY));
    changed.x1 = std::max(changed.x1, std::min<std::int64_t>(image.width(), std::int64_t(blend.width) - offsetX));
    changed.y1 = std::max(changed.y1, std::min<std::int64_t>(image.height(), std::int64_t(blend.height) - offsetY));
  }
  auto apply = [&]<int Mode>()
    requires blend_kernel_mode<Mode>
  {
    auto blendPixel = [&](unsigned x, unsigned y, Vec4B& pixel) {
      std::int64_t px = std::int64_t(x) + offsetX, py = std::int64_t(y) + offsetY;
      Vec4F fpixel = Color::v4bToFloat(pixel);
      for (auto const& blend : sources) {
        if (px >= 0 && py >= 0 && px < blend.width && py < blend.height) {
          Vec4F other = Color::v4bToFloat(blend.read(px, py));
          if constexpr (Mode == BlendImageOperation::Multiply)
            fpixel = fpixel.piecewise_multiply(other);
          else if constexpr (Mode == BlendImageOperation::Screen)
            fpixel =
              Vec4F::filled(1.0f) - (Vec4F::filled(1.0f) - fpixel).piecewise_multiply(Vec4F::filled(1.0f) - other);
        }
      }
      pixel = Color::v4fToByte(fpixel);
    };
    if (image.bytes_per_pixel() == 4) {
      auto data = image.data();
      auto stride = std::size_t(image.width()) * 4;
      for (auto y = changed.y0; y < changed.y1; ++y) {
        auto row = data.subspan(std::size_t(y) * stride + std::size_t(changed.x0) * 4,
                                std::size_t(changed.x1 - changed.x0) * 4);
        auto destination = row.begin();
        for (auto x = changed.x0; x < changed.x1; ++x, std::advance(destination, 4)) {
          Vec4B pixel;
          std::ranges::copy_n(destination, 4, pixel.begin());
          blendPixel(x, y, pixel);
          std::ranges::copy(pixel, destination);
        }
      }
    } else {
      image.for_each_pixel(blendPixel);
    }
  };
  if (op.mode == BlendImageOperation::Multiply)
    apply.template operator()<BlendImageOperation::Multiply>();
  else if (op.mode == BlendImageOperation::Screen)
    apply.template operator()<BlendImageOperation::Screen>();
  else
    apply.template operator()<-1>();
  return changed;
}

std::expected<void, StarException> process_image_operation(ImageOperation const& operation, Image& image,
                                                           ImageReferenceCallback refCallback) {
  if (auto error = (std::holds_alternative<ErrorImageOperation>(operation) ? std::optional{std::cref(std::get<ErrorImageOperation>(operation))} : std::nullopt))
    return std::unexpected(error->get().cause);
  if (image.bytes_per_pixel() == 3) {
    // Convert to an image format that has alpha so certain operations function properly
    image = image.convert(image.pixel_format() == PixelFormat::BGR24 ? PixelFormat::BGRA32 : PixelFormat::RGBA32);
  }
  if (auto op = (std::holds_alternative<HueShiftImageOperation>(operation) ? std::optional{std::cref(std::get<HueShiftImageOperation>(operation))} : std::nullopt)) {
    // Exact identities preserve the input bytes, rather than round-tripping HSV.
    // Keep these after the 24-to-32-bit conversion above.
    if (op->get().hueShiftAmount == 0.0f)
      return {};
    RecentColors recent;
    image.for_each_pixel([&](unsigned, unsigned, Vec4B& pixel) {
      if (pixel[3] != 0)
        recent.apply(pixel, [&](Vec4B const& input) { return Color::hueShiftVec4B(input, op->get().hueShiftAmount); });
    });
  } else if (auto op = (std::holds_alternative<SaturationShiftImageOperation>(operation) ? std::optional{std::cref(std::get<SaturationShiftImageOperation>(operation))} : std::nullopt)) {
    if (op->get().saturationShiftAmount != 0.0f)
      process_saturation_shift(image, op->get());
  } else if (auto op = (std::holds_alternative<BrightnessMultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<BrightnessMultiplyImageOperation>(operation))} : std::nullopt)) {
    if (op->get().brightnessMultiply == 1.0f)
      return {};
    RecentColors recent;
    image.for_each_pixel([&](unsigned, unsigned, Vec4B& pixel) {
      if (pixel[3] != 0) {
        recent.apply(pixel, [&](Vec4B const& input) {
          Color color = Color::rgba(input);
          color.setValue(std::clamp(color.value() * op->get().brightnessMultiply, 0.0f, 1.0f));
          return color.toRgba();
        });
      }
    });
  } else if (auto op = (std::holds_alternative<FadeToColorImageOperation>(operation) ? std::optional{std::cref(std::get<FadeToColorImageOperation>(operation))} : std::nullopt)) {
    image.for_each_pixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel[0] = op->get().rTable[pixel[0]];
      pixel[1] = op->get().gTable[pixel[1]];
      pixel[2] = op->get().bTable[pixel[2]];
    });
  } else if (auto op = (std::holds_alternative<ScanLinesImageOperation>(operation) ? std::optional{std::cref(std::get<ScanLinesImageOperation>(operation))} : std::nullopt)) {
    image.for_each_pixel([&op](unsigned, unsigned y, Vec4B& pixel) {
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
    image.for_each_pixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel[0] = op->get().color[0];
      pixel[1] = op->get().color[1];
      pixel[2] = op->get().color[2];
    });
  } else if (auto op = (std::holds_alternative<ColorReplaceImageOperation>(operation) ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))} : std::nullopt)) {
    auto const& replaceMap = op->get().colorReplaceMap;
    if (replaceMap.empty())
      return {};
    if (image.bytes_per_pixel() == 4)
      replace_colors(replaceMap, image, {.x0 = 0, .y0 = 0, .x1 = image.width(), .y1 = image.height()});
    else
      image.for_each_pixel([&](unsigned, unsigned, Vec4B& pixel) {
        if (auto replacement = replaceMap.maybe(pixel))
          pixel = *replacement;
      });

  } else if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt)) {
    if (op->get().maskImages.empty())
      return {};

    if (!refCallback)
      return std::unexpected(ImageOperationException("Missing image ref callback during AlphaMaskImageOperation"));

    bool additive = op->get().mode == AlphaMaskImageOperation::Additive;
    List<PreparedImageReference> sources;
    if (additive)
      sources.reserve(op->get().maskImages.size());
    for (auto const& reference : op->get().maskImages) {
      auto mask = refCallback(reference);
      if (!mask)
        return std::unexpected(std::move(mask.error()));
      if (!*mask)
        return std::unexpected(ImageOperationException::format("Missing mask image '{}'", reference));
      if (additive)
        sources.emplace_back((**mask).get());
    }

    // The legacy subtractive reduction starts at zero and therefore stays zero.
    // Resolve every source above even when no alpha reads are necessary.
    for (auto& source : sources)
      source.prepare();
    auto offset = Vec2U(op->get().offset);
    auto maskAlpha = [&](unsigned x, unsigned y, std::uint8_t alpha) {
      std::uint8_t maximum = 0;
      auto pos = Vec2U(x, y) + offset;
      for (auto const& mask : sources) {
        if (pos[0] < mask.width && pos[1] < mask.height)
          maximum = std::max(maximum, mask.alpha(pos));
      }
      return std::min(alpha, maximum);
    };
    if (image.bytes_per_pixel() == 4) {
      auto data = image.data();
      auto stride = std::size_t(image.width()) * 4;
      for (unsigned y = 0; y < image.height(); ++y) {
        auto row = data.subspan(std::size_t(y) * stride, stride);
        auto destination = row.begin();
        for (unsigned x = 0; x < image.width(); ++x, std::advance(destination, 4)) {
          destination[3] = maskAlpha(x, y, destination[3]);
        }
      }
    } else {
      image.for_each_pixel([&](unsigned x, unsigned y, Vec4B& pixel) { pixel[3] = maskAlpha(x, y, pixel[3]); });
    }

  } else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt)) {
    auto blended = apply_blend(op->get(), image, refCallback);
    if (!blended)
      return std::unexpected(std::move(blended.error()));

  } else if (auto op = (std::holds_alternative<MultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<MultiplyImageOperation>(operation))} : std::nullopt)) {
    image.for_each_pixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel = pixel.combine(op->get().color, [](std::uint8_t a, std::uint8_t b) -> std::uint8_t {
        return static_cast<std::uint8_t>((static_cast<int>(a) * static_cast<int>(b)) / 255);
      });
    });

  } else if (auto op = (std::holds_alternative<BorderImageOperation>(operation) ? std::optional{std::cref(std::get<BorderImageOperation>(operation))} : std::nullopt)) {
    auto extent =
      Vector<std::uint64_t, 2>(image.size()) + Vector<std::uint64_t, 2>::filled(std::uint64_t(op->get().pixels) * 2);
    if (extent[0] > std::numeric_limits<int>::max() || extent[1] > std::numeric_limits<int>::max())
      return std::unexpected(ImageOperationException("Border image extent is not representable"));
    auto borderSize = Vec2U(extent);
    if (!Image::size_bytes(borderSize, PixelFormat::RGBA32))
      return std::unexpected(ImageOperationException("Border image byte size is not representable"));
    Image borderImage(borderSize, PixelFormat::RGBA32);
    auto copied = borderImage.copy_into(Vec2U::filled(op->get().pixels), image);
    if (!copied)
      return std::unexpected(std::move(copied.error()));
    Vec2I borderImageSize = Vec2I(borderImage.size());

    borderImage.for_each_pixel([&op, &image, &borderImageSize](int x, int y, Vec4B& pixel) {
      int pixels = op->get().pixels;
      bool includeTransparent = op->get().includeTransparent;
      if (pixel[3] == 0 || (includeTransparent && pixel[3] != 255)) {
        auto dist = std::numeric_limits<std::int64_t>::max();
        for (std::int64_t j = -std::int64_t(pixels); j <= pixels; ++j) {
          for (std::int64_t i = -std::int64_t(pixels); i <= pixels; ++i) {
            if (i + x >= pixels && j + y >= pixels && i + x < borderImageSize[0] - pixels && j + y < borderImageSize[1] - pixels) {
              Vec4B remotePixel = image.get32(i + x - pixels, j + y - pixels);
              if (remotePixel[3] != 0) {
                dist = std::min(dist, std::abs(i) + std::abs(j));
                if (dist == 1)// Preserve the legacy inner break, including partial-alpha targets.
                  break;
              }
            }
          }
          if (dist == 0 || (dist == 1 && pixel[3] == 0 && image.bytes_per_pixel() == 4))
            break;
        }

        if (dist < std::numeric_limits<std::int64_t>::max()) {
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

    image = std::move(borderImage);

  } else if (auto op = (std::holds_alternative<ScaleImageOperation>(operation) ? std::optional{std::cref(std::get<ScaleImageOperation>(operation))} : std::nullopt)) {
    auto scale = op->get().scale;
    if (scale[0] < 0.0f || scale[1] < 0.0f) {
      EmergencyLog::warn("Negative scale in ScaleImageOperation ({})", scale);
      scale = scale.piecewise_max(Vec2F::filled(0.f));
    }
    auto scaled = op->get().mode == ScaleImageOperation::Nearest ? scale_nearest(image, scale)
      : op->get().mode == ScaleImageOperation::Bicubic           ? scale_bicubic(image, scale)
                                                                 : scale_bilinear(image, scale);
    if (!scaled)
      return std::unexpected(std::move(scaled.error()));
    image = std::move(*scaled);

  } else if (auto op = (std::holds_alternative<CropImageOperation>(operation) ? std::optional{std::cref(std::get<CropImageOperation>(operation))} : std::nullopt)) {
    auto cropped = image.sub_image(Vec2U(op->get().subset.min()), Vec2U(op->get().subset.size()));
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

static bool is_per_pixel_color_operation(ImageOperation const& operation) {
  return std::holds_alternative<HueShiftImageOperation>(operation)
    || std::holds_alternative<SaturationShiftImageOperation>(operation)
    || std::holds_alternative<BrightnessMultiplyImageOperation>(operation)
    || std::holds_alternative<FadeToColorImageOperation>(operation)
    || std::holds_alternative<SetColorImageOperation>(operation)
    || std::holds_alternative<ColorReplaceImageOperation>(operation)
    || std::holds_alternative<MultiplyImageOperation>(operation);
}

static bool has_byte_pixels(Image const& image) { return image.bytes_per_pixel() == 3 || image.bytes_per_pixel() == 4; }

static bool scale_enlarges_by_copying(ScaleImageOperation const& scale, Image const& image) {
  // Empty scales synthesize black pixels, rather than copying source pixels.
  // Float images also have different scalar byte-setter semantics.
  if (image.empty() || !has_byte_pixels(image) || scale.mode != ScaleImageOperation::Nearest
      || !scaled_crop_fits(image, scale.scale, RectI(0, 0, 0, 0)))
    return false;
  auto size = Vec2U::round(vmult(Vec2F(image.size()), scale.scale));
  size[0] = std::max(size[0], 1u);
  size[1] = std::max(size[1], 1u);
  return std::uint64_t(size[0]) * size[1] > std::uint64_t(image.width()) * image.height();
}

static bool crop_shrinks(CropImageOperation const& crop, Image const& image) {
  auto min = crop.subset.min(), max = crop.subset.max();
  if (!has_byte_pixels(image) || min[0] < 0 || min[1] < 0 || max[0] < min[0] || max[1] < min[1]
      || std::uint64_t(max[0]) > image.width() || std::uint64_t(max[1]) > image.height())
    return false;
  return std::uint64_t(max[0] - min[0]) * (max[1] - min[1]) < std::uint64_t(image.width()) * image.height();
}

static std::expected<bool, StarException> process_scale_crop(ScaleImageOperation const& scale,
                                                             CropImageOperation const& crop, Image& image) {
  // Leave invalid crops/scales and float formats to the sequential error path.
  if (!has_byte_pixels(image) || scale.mode == ScaleImageOperation::Bicubic
      || !scaled_crop_fits(image, scale.scale, crop.subset))
    return false;
  if (image.bytes_per_pixel() == 3)
    image = image.convert(image.pixel_format() == PixelFormat::BGR24 ? PixelFormat::BGRA32 : PixelFormat::RGBA32);
  auto scaled = scale.mode == ScaleImageOperation::Nearest ? scale_nearest_cropped(image, scale.scale, crop.subset)
                                                           : scale_bilinear_cropped(image, scale.scale, crop.subset);
  if (!scaled)
    return std::unexpected(std::move(scaled.error()));
  image = std::move(*scaled);
  return true;
}

static bool replace_settles(ColorReplaceMap const& map) {
  for (auto const& entry : map) {
    if (map.contains(entry.second))
      return false;
  }
  return true;
}

static bool replace_keys_are_keys_of(ColorReplaceMap const& map, ColorReplaceMap const& other) {
  for (auto const& entry : map) {
    if (!other.contains(entry.first))
      return false;
  }
  return true;
}

template <typename OperationAt>
concept image_operation_accessor = requires(OperationAt& operation_at, std::size_t index) {
  { operation_at(index) } -> std::same_as<ImageOperation const&>;
};

static bool is_separable_color_operation(ImageOperation const& operation) {
  return std::holds_alternative<FadeToColorImageOperation>(operation)
    || std::holds_alternative<SetColorImageOperation>(operation)
    || std::holds_alternative<MultiplyImageOperation>(operation)
    || std::holds_alternative<ScanLinesImageOperation>(operation);
}

static bool is_color_operation(ImageOperation const& operation) {
  return is_per_pixel_color_operation(operation) || std::holds_alternative<ScanLinesImageOperation>(operation);
}

template <image_operation_accessor OperationAt>
static std::expected<void, StarException> process_color_segment(std::size_t begin, std::size_t end,
                                                                OperationAt& operation_at, Image& image,
                                                                ImageReferenceCallback const& ref_callback) {
  for (auto i = begin; i < end;) {
    auto limit = i;
    while (limit < end && is_separable_color_operation(operation_at(limit)))
      ++limit;
    if (limit - i < 2 || !has_byte_pixels(image)) {
      auto processed = process_image_operation(operation_at(i), image, ref_callback);
      if (!processed)
        return std::unexpected(std::move(processed.error()));
      ++i;
      continue;
    }
    if (image.bytes_per_pixel() == 3)
      image = image.convert(image.pixel_format() == PixelFormat::BGR24 ? PixelFormat::BGRA32 : PixelFormat::RGBA32);

    // Compose byte-to-byte tables, not floating operations: every intermediate
    // truncation remains observable. Both row parities use absolute image y.
    std::uint8_t tables[2][4][256];
    for (auto& parity : tables)
      for (auto& channel : parity)
        for (unsigned value = 0; value < 256; ++value)
          channel[value] = value;
    for (auto k = i; k < limit; ++k) {
      [[maybe_unused]] auto operation_callback = ref_callback;
      auto const& operation = operation_at(k);
      for (unsigned parity = 0; parity < 2; ++parity) {
        std::optional<std::reference_wrapper<FadeToColorImageOperation const>> fade;
        if (std::holds_alternative<FadeToColorImageOperation>(operation))
          fade = std::cref(std::get<FadeToColorImageOperation>(operation));
        if (std::holds_alternative<ScanLinesImageOperation>(operation)) {
          auto const& scanlines = std::get<ScanLinesImageOperation>(operation);
          fade = std::cref(parity == 0 ? scanlines.fade1 : scanlines.fade2);
        }
        if (fade) {
          for (unsigned value = 0; value < 256; ++value) {
            tables[parity][0][value] = fade->get().rTable[tables[parity][0][value]];
            tables[parity][1][value] = fade->get().gTable[tables[parity][1][value]];
            tables[parity][2][value] = fade->get().bTable[tables[parity][2][value]];
          }
        } else if (std::holds_alternative<SetColorImageOperation>(operation)) {
          auto const& set = std::get<SetColorImageOperation>(operation);
          for (unsigned channel = 0; channel < 3; ++channel)
            std::ranges::fill(tables[parity][channel], set.color[channel]);
        } else if (std::holds_alternative<MultiplyImageOperation>(operation)) {
          auto const& multiply = std::get<MultiplyImageOperation>(operation);
          for (unsigned channel = 0; channel < 4; ++channel)
            for (auto& value : tables[parity][channel])
              value = unsigned(value) * multiply.color[channel] / 255;
        }
      }
    }
    auto data = image.data();
    auto stride = std::size_t(image.width()) * 4;
    for (unsigned y = 0; y < image.height(); ++y) {
      auto const& table = tables[y % 2];
      auto row = data.subspan(std::size_t(y) * stride, stride);
      auto pixel = row.begin();
      for (unsigned x = 0; x < image.width(); ++x, std::advance(pixel, 4)) {
        for (unsigned channel = 0; channel < 4; ++channel)
          pixel[channel] = table[channel][pixel[channel]];
      }
    }
    i = limit;
  }
  return {};
}

// An accessor keeps the contiguous List entrypoint allocation-free, while directives
// can refer to stable, already loaded operations without copying their maps/tables.
template <image_operation_accessor OperationAt>
static std::expected<void, StarException> process_image_operation_sequence(std::size_t count, OperationAt operationAt,
                                                                           Image& image,
                                                                           ImageReferenceCallback const& refCallback) {
  std::optional<std::reference_wrapper<ColorReplaceImageOperation const>> settled;
  PixelRect changed;
  bool sawBlend = false;
  for (std::size_t i = 0; i < count;) {
    auto const& operation = operationAt(i);

    if (std::holds_alternative<BlendImageOperation>(operation) && settled) {
      auto const& blend = std::get<BlendImageOperation>(operation);
      // Match process_image_operation's per-operation callback value semantics.
      auto operationCallback = refCallback;
      auto blended = apply_blend(blend, image, operationCallback);
      if (!blended)
        return std::unexpected(std::move(blended.error()));
      changed.include(*blended);
      sawBlend = true;
      ++i;
      continue;
    }
    if (auto replace = (std::holds_alternative<ColorReplaceImageOperation>(operation)
                          ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))}
                          : std::nullopt);
        replace && settled && sawBlend
        && replace_keys_are_keys_of(replace->get().colorReplaceMap, settled->get().colorReplaceMap)
        && replace_settles(replace->get().colorReplaceMap)) {
      // Outside the consecutive blends' union there are no keys left to replace.
      replace_colors(replace->get().colorReplaceMap, image, changed);
      settled = replace;
      changed = {};
      sawBlend = false;
      ++i;
      continue;
    }
    settled.reset();
    changed = {};
    sawBlend = false;

    if (is_color_operation(operation)) {
      auto end = i + 1;
      bool canReorderCrop = is_per_pixel_color_operation(operation);
      while (end < count && is_color_operation(operationAt(end))) {
        canReorderCrop = canReorderCrop && is_per_pixel_color_operation(operationAt(end));
        ++end;
      }
      if (canReorderCrop && end < count) {
        if (auto crop = (std::holds_alternative<CropImageOperation>(operationAt(end))
                           ? std::optional{std::cref(std::get<CropImageOperation>(operationAt(end)))}
                           : std::nullopt);
            crop && crop_shrinks(crop->get(), image)) {
          auto cropped = process_image_operation(operationAt(end), image, refCallback);
          if (!cropped)
            return std::unexpected(std::move(cropped.error()));
          auto processed = process_color_segment(i, end, operationAt, image, refCallback);
          if (!processed)
            return std::unexpected(std::move(processed.error()));
          i = end + 1;
          continue;
        }
      }
      auto processed = process_color_segment(i, end, operationAt, image, refCallback);
      if (!processed)
        return std::unexpected(std::move(processed.error()));
      // Only a successfully applied final replace establishes the settled invariant.
      if (image.bytes_per_pixel() == 4 && end < count && std::holds_alternative<BlendImageOperation>(operationAt(end))
          && std::holds_alternative<ColorReplaceImageOperation>(operationAt(end - 1))) {
        auto const& replace = std::get<ColorReplaceImageOperation>(operationAt(end - 1));
        if (replace_settles(replace.colorReplaceMap))
          settled = std::cref(replace);
      }
      i = end;
      continue;
    }

    if (std::holds_alternative<ScaleImageOperation>(operation)) {
      auto const& scale = std::get<ScaleImageOperation>(operation);
      auto end = i + 1;
      while (end < count && is_per_pixel_color_operation(operationAt(end)))
        ++end;
      if (end > i + 1 && scale_enlarges_by_copying(scale, image)) {
        auto processed = process_color_segment(i + 1, end, operationAt, image, refCallback);
        if (!processed)
          return std::unexpected(std::move(processed.error()));
        if (end < count) {
          if (std::holds_alternative<CropImageOperation>(operationAt(end))) {
            auto const& crop = std::get<CropImageOperation>(operationAt(end));
            auto fused = process_scale_crop(scale, crop, image);
            if (!fused)
              return std::unexpected(std::move(fused.error()));
            if (*fused) {
              i = end + 1;
              continue;
            }
          }
        }
        auto scaled = process_image_operation(operation, image, refCallback);
        if (!scaled)
          return std::unexpected(std::move(scaled.error()));
        i = end;
        continue;
      }
      if (i + 1 < count) {
        if (std::holds_alternative<CropImageOperation>(operationAt(i + 1))) {
          auto const& crop = std::get<CropImageOperation>(operationAt(i + 1));
          auto fused = process_scale_crop(scale, crop, image);
          if (!fused)
            return std::unexpected(std::move(fused.error()));
          if (*fused) {
            i += 2;
            continue;
          }
        }
      }
    }
    auto processed = process_image_operation(operation, image, refCallback);
    if (!processed)
      return std::unexpected(std::move(processed.error()));
    ++i;
  }
  return {};
}

std::expected<void, StarException>
process_image_operations(std::span<std::reference_wrapper<ImageOperation const> const> operations, Image& image,
                         ImageReferenceCallback refCallback) {
  return process_image_operation_sequence(
    operations.size(), [&](std::size_t i) -> ImageOperation const& { return operations[i].get(); }, image, refCallback);
}

std::expected<Image, StarException> process_image_operations(List<ImageOperation> const& operations, Image image,
                                                             ImageReferenceCallback refCallback) {
  auto processed = process_image_operation_sequence(
    operations.size(), [&](std::size_t i) -> ImageOperation const& { return operations[i]; }, image, refCallback);
  if (!processed)
    return std::unexpected(std::move(processed.error()));
  return image;
}
}

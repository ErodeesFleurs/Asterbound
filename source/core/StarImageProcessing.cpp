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
    auto text = entry.type() == Json::Type::Object ? palette_swap_directives_from_config(entry) : entry.to_string();
    if (!text)
      return std::unexpected(std::move(text.error()));
    result.push_back(std::move(*text));
  }
  return result;
}

std::expected<String, StarException> palette_swap_directives_from_config(Json const& swaps) {
  ColorReplaceMap palette_swaps;
  auto object = swaps.iterate_object();
  if (!object)
    return std::unexpected(std::move(object.error()));
  for (auto const& swap : *object) {
    auto from = Color::from_hex(swap.first);
    if (!from)
      return std::unexpected(std::move(from.error()));
    auto text = swap.second.to_string();
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto to = Color::from_hex(*text);
    if (!to)
      return std::unexpected(std::move(to.error()));
    palette_swaps[from->to_rgba()] = to->to_rgba();
  }
  return "?" + color_replace_operation_to_string(palette_swaps);
}

HueShiftImageOperation HueShiftImageOperation::hue_shift_degrees(float degrees) {
  return HueShiftImageOperation{positive_mod(degrees / 360.0f, 1.0f)};
}

SaturationShiftImageOperation SaturationShiftImageOperation::saturation_shift_100(float amount) {
  return SaturationShiftImageOperation{amount / 100.0f};
}

BrightnessMultiplyImageOperation BrightnessMultiplyImageOperation::brightness_multiply_100(float amount) {
  return BrightnessMultiplyImageOperation{amount / 100.0f + 1.0f};
}

FadeToColorImageOperation::FadeToColorImageOperation(Vec3B color, float amount)
  : color(color), amount(amount), tables(std::make_shared<ColorLookupTables const>(color, amount)) {}

ColorLookupTables::ColorLookupTables(Vec3B color, float amount) {
  auto fcl = Color::rgb(color).to_linear();
  // Decode through the byte Color path once, retaining its exact rounding.
  static std::array<float, 256> const decoded_gray = [] {
    std::array<float, 256> values;
    for (int i = 0; i <= 255; ++i)
      values[i] = Color::rgb(Vec3B(i, i, i)).to_linear().red_f();
    return values;
  }();
  for (int i = 0; i <= 255; ++i) {
    float gray = decoded_gray[i];
    auto r = Color::rgb_f(gray, gray, gray).mix(fcl, amount).to_srgb().to_rgb();
    r_table[i] = r[0];
    g_table[i] = r[1];
    b_table[i] = r[2];
  }
}

ImageOperation image_operation_from_string(StringView string) {
  // Directives have an ASCII grammar; preserve non-ASCII reference bytes.
  std::string_view text = string.utf8();
  auto first_delimiter = text.find_first_of("=;");
  if (text.substr(0, first_delimiter) == "replace") {
    ColorReplaceImageOperation operation;
    if (first_delimiter == std::string_view::npos)
      return operation;
    operation.color_replace_map.reserve((text.size() - first_delimiter) / 8);
    std::optional<StringView> source;
    for (std::size_t begin = first_delimiter + 1; begin < text.size();) {
      auto end = text.find_first_of("=;", begin);
      if (end == std::string_view::npos)
        end = text.size();
      if (end != begin) {
        StringView hex(text.substr(begin, end - begin));
        if (!source)
          source = hex;
        else {
          auto from = Color::parse_rgba_hex(*source);
          if (!from)
            return ErrorImageOperation{.cause = std::move(from.error())};
          auto to = Color::parse_rgba_hex(hex);
          if (!to)
            return ErrorImageOperation{.cause = std::move(to.error())};
          operation.color_replace_map[*from] = *to;
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
    auto color = Color::from_hex(bits[1]);
    if (!color)
      return error(std::move(color.error()));
    if (type == "setcolor")
      return SetColorImageOperation{color->to_rgb()};
    return MultiplyImageOperation{color->to_rgba()};
  }
  if (type == "fade" || type == "scanlines") {
    auto color = Color::from_hex(bits[1]);
    if (!color)
      return error(std::move(color.error()));
    auto amount = lexical_cast<float>(bits[2]);
    if (!amount)
      return error(std::move(amount.error()));
    FadeToColorImageOperation fade(color->to_rgb(), *amount);
    if (type == "fade")
      return fade;
    auto second_color = Color::from_hex(bits[3]);
    if (!second_color)
      return error(std::move(second_color.error()));
    auto second_amount = lexical_cast<float>(bits[4]);
    if (!second_amount)
      return error(std::move(second_amount.error()));
    return ScanLinesImageOperation{.fade1 = fade,
                                   .fade2 = FadeToColorImageOperation(second_color->to_rgb(), *second_amount)};
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
                                     .mask_images = std::move(references),
                                     .offset = offset};
    return BlendImageOperation{.mode =
                                 type == "blendmult" ? BlendImageOperation::Multiply : BlendImageOperation::Screen,
                               .blend_images = std::move(references),
                               .offset = offset};
  }
  if (type == "border" || type == "outline") {
    auto pixels = lexical_cast<unsigned>(bits[1]);
    if (!pixels)
      return error(std::move(pixels.error()));
    auto start = Color::from_hex(bits[2]);
    if (!start)
      return error(std::move(start.error()));
    auto end = bits.size() > 3 ? Color::from_hex(bits[3]) : start;
    if (!end)
      return error(std::move(end.error()));
    return BorderImageOperation{.pixels = *pixels,
                                .start_color = start->to_rgba(),
                                .end_color = end->to_rgba(),
                                .outline_only = type == "outline",
                                .include_transparent = false};
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

String color_replace_operation_to_string(ColorReplaceMap const& swaps) {
  String str = "replace";
  for (auto const& [from, to] : swaps)
    str += Star::format(";{}={}", Color::rgba(from).to_hex(), Color::rgba(to).to_hex());
  return str;
}

String image_operation_to_string(ImageOperation const& operation) {
  if (auto op = (std::holds_alternative<HueShiftImageOperation>(operation) ? std::optional{std::cref(std::get<HueShiftImageOperation>(operation))} : std::nullopt)) {
    return Star::format("hueshift={}", op->get().hue_shift_amount * 360.0f);
  } else if (auto op = (std::holds_alternative<SaturationShiftImageOperation>(operation) ? std::optional{std::cref(std::get<SaturationShiftImageOperation>(operation))} : std::nullopt)) {
    return Star::format("saturation={}", op->get().saturation_shift_amount * 100.0f);
  } else if (auto op = (std::holds_alternative<BrightnessMultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<BrightnessMultiplyImageOperation>(operation))} : std::nullopt)) {
    return Star::format("brightness={}", (op->get().brightness_multiply - 1.0f) * 100.0f);
  } else if (auto op = (std::holds_alternative<FadeToColorImageOperation>(operation) ? std::optional{std::cref(std::get<FadeToColorImageOperation>(operation))} : std::nullopt)) {
    return Star::format("fade={}={}", Color::rgb(op->get().color).to_hex(), op->get().amount);
  } else if (auto op = (std::holds_alternative<ScanLinesImageOperation>(operation) ? std::optional{std::cref(std::get<ScanLinesImageOperation>(operation))} : std::nullopt)) {
    return Star::format("scanlines={}={}={}={}",
        Color::rgb(op->get().fade1.color).to_hex(),
        op->get().fade1.amount,
        Color::rgb(op->get().fade2.color).to_hex(),
        op->get().fade2.amount);
  } else if (auto op = (std::holds_alternative<SetColorImageOperation>(operation) ? std::optional{std::cref(std::get<SetColorImageOperation>(operation))} : std::nullopt)) {
    return Star::format("setcolor={}", Color::rgb(op->get().color).to_hex());
  } else if (auto op = (std::holds_alternative<ColorReplaceImageOperation>(operation) ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))} : std::nullopt)) {
    return color_replace_operation_to_string(op->get().color_replace_map);
  } else if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == AlphaMaskImageOperation::Additive)
      return Star::format("addmask={};{};{}", op->get().mask_images.join("+"), op->get().offset[0], op->get().offset[1]);
    else if (op->get().mode == AlphaMaskImageOperation::Subtractive)
      return Star::format("submask={};{};{}", op->get().mask_images.join("+"), op->get().offset[0], op->get().offset[1]);
  } else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == BlendImageOperation::Multiply)
      return Star::format("blendmult={};{};{}", op->get().blend_images.join("+"), op->get().offset[0], op->get().offset[1]);
    else if (op->get().mode == BlendImageOperation::Screen)
      return Star::format("blendscreen={};{};{}", op->get().blend_images.join("+"), op->get().offset[0], op->get().offset[1]);
  } else if (auto op = (std::holds_alternative<MultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<MultiplyImageOperation>(operation))} : std::nullopt)) {
    return Star::format("multiply={}", Color::rgba(op->get().color).to_hex());
  } else if (auto op = (std::holds_alternative<BorderImageOperation>(operation) ? std::optional{std::cref(std::get<BorderImageOperation>(operation))} : std::nullopt)) {
    if (op->get().outline_only)
      return Star::format("outline={};{};{}", op->get().pixels, Color::rgba(op->get().start_color).to_hex(), Color::rgba(op->get().end_color).to_hex());
    else
      return Star::format("border={};{};{}", op->get().pixels, Color::rgba(op->get().start_color).to_hex(), Color::rgba(op->get().end_color).to_hex());
  } else if (auto op = (std::holds_alternative<ScaleImageOperation>(operation) ? std::optional{std::cref(std::get<ScaleImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mode == ScaleImageOperation::Nearest)
      return Star::format("scalenearest={}", op->get().scale);
    else if (op->get().mode == ScaleImageOperation::Bilinear)
      return Star::format("scalebilinear={}", op->get().scale);
    else if (op->get().mode == ScaleImageOperation::Bicubic)
      return Star::format("scalebicubic={}", op->get().scale);
  } else if (auto op = (std::holds_alternative<CropImageOperation>(operation) ? std::optional{std::cref(std::get<CropImageOperation>(operation))} : std::nullopt)) {
    return Star::format("crop={};{};{};{}", op->get().subset.x_min(), op->get().subset.x_max(), op->get().subset.y_min(), op->get().subset.y_max());
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
    out.append_range(op->get().mask_images);
  else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt))
    out.append_range(op->get().blend_images);
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
        // Retain getter/setter arithmetic: HSV-tuple reuse changes half-byte rounding under fast-math.
        Color color = Color::rgba(input);
        color.set_saturation(std::clamp(color.saturation() + op.saturation_shift_amount, 0.0f, 1.0f));
        return color.to_rgba();
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
static void replace_colors(ColorReplaceMap const& replace_map, Image& image, PixelRect rect) {
  if (replace_map.empty() || rect.empty())
    return;
  if (std::uint64_t(rect.x1 - rect.x0) * (rect.y1 - rect.y0) < 1024) {
    for (auto y = rect.y0; y < rect.y1; ++y) {
      for (auto x = rect.x0; x < rect.x1; ++x) {
        auto pixel = image.get32(x, y);
        if (auto replacement = replace_map.maybe(pixel))
          image.set32(x, y, *replacement);
      }
    }
    return;
  }

  unsigned bit_count = 4096;
  while (replace_map.size() > bit_count / 32 && bit_count < (1u << 20))
    bit_count <<= 1;
  unsigned shift = 32 - std::countr_zero(bit_count);
  std::array<std::uint64_t, 64> small_bits{};
  std::vector<std::uint64_t> large_bits;
  std::span<std::uint64_t> bits = small_bits;
  if (bit_count > 4096) {
    large_bits.resize(bit_count / 64);
    bits = large_bits;
  }
  auto bit_for = [shift](Vec4B const& pixel) { return (RecentColors::pack(pixel) * 2654435761u) >> shift; };
  for (auto const& entry : replace_map) {
    auto bit = bit_for(entry.first);
    bits[bit >> 6] |= std::uint64_t(1) << (bit & 63);
  }

  static_assert(sizeof(Vec4B) == sizeof(std::uint32_t));
  auto data = image.span();
  std::uint32_t last_in = 0, last_out = 0;
  bool have_last = false;
  for (auto y = rect.y0; y < rect.y1; ++y) {
    auto row = data.subspan((std::size_t(y) * image.width() + rect.x0) * 4, std::size_t(rect.x1 - rect.x0) * 4);
    auto destination = row.begin();
    for (auto x = rect.x0; x < rect.x1; ++x, std::advance(destination, 4)) {
      std::array<std::uint8_t, 4> input_bytes;
      std::ranges::copy_n(destination, 4, input_bytes.begin());
      auto input = std::bit_cast<std::uint32_t>(input_bytes);
      if (have_last && input == last_in) {
        std::ranges::copy(std::bit_cast<std::array<std::uint8_t, 4>>(last_out), destination);
        continue;
      }
      Vec4B pixel;
      std::ranges::copy(input_bytes, pixel.begin());
      auto output = input;
      auto bit = bit_for(pixel);
      if ((bits[bit >> 6] >> (bit & 63)) & 1) {
        if (auto replacement = replace_map.maybe(pixel)) {
          std::ranges::copy(*replacement, input_bytes.begin());
          output = std::bit_cast<std::uint32_t>(input_bytes);
          std::ranges::copy(input_bytes, destination);
        }
      }
      last_in = input;
      last_out = output;
      have_last = true;
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
    data = image.get().span().begin();
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

template <int blend_mode>
concept blend_kernel_mode = blend_mode == BlendImageOperation::Multiply || blend_mode == BlendImageOperation::Screen || blend_mode == -1;

// Resolve only this operation's references, before changing any of its pixels.
static std::expected<PixelRect, StarException> apply_blend(BlendImageOperation const& op, Image& image,
                                                           ImageReferenceCallback const& ref_callback) {
  if (op.blend_images.empty())
    return PixelRect{};
  if (!ref_callback)
    return std::unexpected(ImageOperationException("Missing image ref callback during BlendImageOperation"));

  List<PreparedImageReference> sources;
  sources.reserve(op.blend_images.size());
  for (auto const& reference : op.blend_images) {
    auto blend = ref_callback(reference);
    if (!blend)
      return std::unexpected(std::move(blend.error()));
    if (!*blend)
      return std::unexpected(ImageOperationException::format("Missing blend image '{}'", reference));
    sources.emplace_back((**blend).get());
  }

  for (auto& source : sources)
    source.prepare();

  std::int64_t offset_x = op.offset[0], offset_y = op.offset[1];
  PixelRect changed{.x0 = image.width(), .y0 = image.height(), .x1 = 0, .y1 = 0};
  for (auto const& blend : sources) {
    changed.x0 = std::min(changed.x0, std::max<std::int64_t>(0, -offset_x));
    changed.y0 = std::min(changed.y0, std::max<std::int64_t>(0, -offset_y));
    changed.x1 = std::max(changed.x1, std::min<std::int64_t>(image.width(), std::int64_t(blend.width) - offset_x));
    changed.y1 = std::max(changed.y1, std::min<std::int64_t>(image.height(), std::int64_t(blend.height) - offset_y));
  }
  auto apply = [&]<int blend_mode>()
    requires blend_kernel_mode<blend_mode>
  {
    auto blend_pixel = [&](unsigned x, unsigned y, Vec4B& pixel) {
      std::int64_t px = std::int64_t(x) + offset_x, py = std::int64_t(y) + offset_y;
      Vec4F fpixel = Color::rgba_bytes_to_float(pixel);
      for (auto const& blend : sources) {
        if (px >= 0 && py >= 0 && px < blend.width && py < blend.height) {
          Vec4F other = Color::rgba_bytes_to_float(blend.read(px, py));
          if constexpr (blend_mode == BlendImageOperation::Multiply)
            fpixel = fpixel.piecewise_multiply(other);
          else if constexpr (blend_mode == BlendImageOperation::Screen)
            fpixel =
              Vec4F::filled(1.0f) - (Vec4F::filled(1.0f) - fpixel).piecewise_multiply(Vec4F::filled(1.0f) - other);
        }
      }
      pixel = Color::rgba_float_to_bytes(fpixel);
    };
    if (image.bytes_per_pixel() == 4) {
      auto data = image.span();
      auto stride = std::size_t(image.width()) * 4;
      for (auto y = changed.y0; y < changed.y1; ++y) {
        auto row = data.subspan(std::size_t(y) * stride + std::size_t(changed.x0) * 4,
                                std::size_t(changed.x1 - changed.x0) * 4);
        auto destination = row.begin();
        for (auto x = changed.x0; x < changed.x1; ++x, std::advance(destination, 4)) {
          Vec4B pixel;
          std::ranges::copy_n(destination, 4, pixel.begin());
          blend_pixel(x, y, pixel);
          std::ranges::copy(pixel, destination);
        }
      }
    } else {
      image.for_each_pixel(blend_pixel);
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

std::expected<Vec2U, StarException> bordered_image_size(Vec2U const& size, unsigned pixels) {
  auto extent = Vector<std::uint64_t, 2>(size) + Vector<std::uint64_t, 2>::filled(std::uint64_t(pixels) * 2);
  if (extent[0] > std::numeric_limits<int>::max() || extent[1] > std::numeric_limits<int>::max())
    return std::unexpected(ImageOperationException("Border image extent is not representable"));
  auto border_size = Vec2U(extent);
  if (!Image::size_bytes(border_size, PixelFormat::RGBA32))
    return std::unexpected(ImageOperationException("Border image byte size is not representable"));
  return border_size;
}

std::expected<void, StarException> process_image_operation(ImageOperation const& operation, Image& image,
                                                           ImageReferenceCallback ref_callback) {
  if (auto error = (std::holds_alternative<ErrorImageOperation>(operation) ? std::optional{std::cref(std::get<ErrorImageOperation>(operation))} : std::nullopt))
    return std::unexpected(error->get().cause);
  if (image.bytes_per_pixel() == 3) {
    // Convert to an image format that has alpha so certain operations function properly
    image = image.convert(image.pixel_format() == PixelFormat::BGR24 ? PixelFormat::BGRA32 : PixelFormat::RGBA32);
  }
  if (auto op = (std::holds_alternative<HueShiftImageOperation>(operation) ? std::optional{std::cref(std::get<HueShiftImageOperation>(operation))} : std::nullopt)) {
    // Exact identities preserve the input bytes, rather than round-tripping HSV.
    // Keep these after the 24-to-32-bit conversion above.
    if (op->get().hue_shift_amount == 0.0f)
      return {};
    RecentColors recent;
    image.for_each_pixel([&](unsigned, unsigned, Vec4B& pixel) {
      if (pixel[3] != 0)
        recent.apply(pixel, [&](Vec4B const& input) { return Color::hue_shift_rgba(input, op->get().hue_shift_amount); });
    });
  } else if (auto op = (std::holds_alternative<SaturationShiftImageOperation>(operation) ? std::optional{std::cref(std::get<SaturationShiftImageOperation>(operation))} : std::nullopt)) {
    if (op->get().saturation_shift_amount != 0.0f)
      process_saturation_shift(image, op->get());
  } else if (auto op = (std::holds_alternative<BrightnessMultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<BrightnessMultiplyImageOperation>(operation))} : std::nullopt)) {
    if (op->get().brightness_multiply == 1.0f)
      return {};
    RecentColors recent;
    image.for_each_pixel([&](unsigned, unsigned, Vec4B& pixel) {
      if (pixel[3] != 0) {
        recent.apply(pixel, [&](Vec4B const& input) {
          auto hsva = Color::rgba(input).to_hsva();
          hsva[2] = std::clamp(hsva[2] * op->get().brightness_multiply, 0.0f, 1.0f);
          return Color::hsva(hsva).to_rgba();
        });
      }
    });
  } else if (auto op = (std::holds_alternative<FadeToColorImageOperation>(operation) ? std::optional{std::cref(std::get<FadeToColorImageOperation>(operation))} : std::nullopt)) {
    auto const& tables = op->get().color_tables();
    image.for_each_pixel([&tables](unsigned, unsigned, Vec4B& pixel) {
      pixel[0] = tables.r_table[pixel[0]];
      pixel[1] = tables.g_table[pixel[1]];
      pixel[2] = tables.b_table[pixel[2]];
    });
  } else if (auto op = (std::holds_alternative<ScanLinesImageOperation>(operation) ? std::optional{std::cref(std::get<ScanLinesImageOperation>(operation))} : std::nullopt)) {
    auto const& even_tables = op->get().fade1.color_tables();
    auto const& odd_tables = op->get().fade2.color_tables();
    image.for_each_pixel([&even_tables, &odd_tables](unsigned, unsigned y, Vec4B& pixel) {
      auto const& tables = y % 2 == 0 ? even_tables : odd_tables;
      pixel[0] = tables.r_table[pixel[0]];
      pixel[1] = tables.g_table[pixel[1]];
      pixel[2] = tables.b_table[pixel[2]];
    });
  } else if (auto op = (std::holds_alternative<SetColorImageOperation>(operation) ? std::optional{std::cref(std::get<SetColorImageOperation>(operation))} : std::nullopt)) {
    image.for_each_pixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel[0] = op->get().color[0];
      pixel[1] = op->get().color[1];
      pixel[2] = op->get().color[2];
    });
  } else if (auto op = (std::holds_alternative<ColorReplaceImageOperation>(operation) ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))} : std::nullopt)) {
    auto const& replace_map = op->get().color_replace_map;
    if (replace_map.empty())
      return {};
    if (image.bytes_per_pixel() == 4)
      replace_colors(replace_map, image, {.x0 = 0, .y0 = 0, .x1 = image.width(), .y1 = image.height()});
    else
      image.for_each_pixel([&](unsigned, unsigned, Vec4B& pixel) {
        if (auto replacement = replace_map.maybe(pixel))
          pixel = *replacement;
      });

  } else if (auto op = (std::holds_alternative<AlphaMaskImageOperation>(operation) ? std::optional{std::cref(std::get<AlphaMaskImageOperation>(operation))} : std::nullopt)) {
    if (op->get().mask_images.empty())
      return {};

    if (!ref_callback)
      return std::unexpected(ImageOperationException("Missing image ref callback during AlphaMaskImageOperation"));

    bool additive = op->get().mode == AlphaMaskImageOperation::Additive;
    List<PreparedImageReference> sources;
    if (additive)
      sources.reserve(op->get().mask_images.size());
    for (auto const& reference : op->get().mask_images) {
      auto mask = ref_callback(reference);
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
    auto mask_alpha = [&](unsigned x, unsigned y, std::uint8_t alpha) {
      std::uint8_t maximum = 0;
      auto pos = Vec2U(x, y) + offset;
      for (auto const& mask : sources) {
        if (pos[0] < mask.width && pos[1] < mask.height)
          maximum = std::max(maximum, mask.alpha(pos));
      }
      return std::min(alpha, maximum);
    };
    if (image.bytes_per_pixel() == 4) {
      auto data = image.span();
      auto stride = std::size_t(image.width()) * 4;
      for (unsigned y = 0; y < image.height(); ++y) {
        auto row = data.subspan(std::size_t(y) * stride, stride);
        auto destination = row.begin();
        for (unsigned x = 0; x < image.width(); ++x, std::advance(destination, 4)) {
          destination[3] = mask_alpha(x, y, destination[3]);
        }
      }
    } else {
      image.for_each_pixel([&](unsigned x, unsigned y, Vec4B& pixel) { pixel[3] = mask_alpha(x, y, pixel[3]); });
    }

  } else if (auto op = (std::holds_alternative<BlendImageOperation>(operation) ? std::optional{std::cref(std::get<BlendImageOperation>(operation))} : std::nullopt)) {
    auto blended = apply_blend(op->get(), image, ref_callback);
    if (!blended)
      return std::unexpected(std::move(blended.error()));

  } else if (auto op = (std::holds_alternative<MultiplyImageOperation>(operation) ? std::optional{std::cref(std::get<MultiplyImageOperation>(operation))} : std::nullopt)) {
    image.for_each_pixel([&op](unsigned, unsigned, Vec4B& pixel) {
      pixel = pixel.combine(op->get().color, [](std::uint8_t a, std::uint8_t b) -> std::uint8_t {
        return static_cast<std::uint8_t>((static_cast<int>(a) * static_cast<int>(b)) / 255);
      });
    });

  } else if (auto op = (std::holds_alternative<BorderImageOperation>(operation) ? std::optional{std::cref(std::get<BorderImageOperation>(operation))} : std::nullopt)) {
    auto border_size = bordered_image_size(image.size(), op->get().pixels);
    if (!border_size)
      return std::unexpected(std::move(border_size.error()));
    Image border_image(*border_size, PixelFormat::RGBA32);
    auto copied = border_image.copy_into(Vec2U::filled(op->get().pixels), image);
    if (!copied)
      return std::unexpected(std::move(copied.error()));
    Vec2I border_image_size = Vec2I(border_image.size());

    border_image.for_each_pixel([&op, &image, &border_image_size](int x, int y, Vec4B& pixel) {
      int pixels = op->get().pixels;
      bool include_transparent = op->get().include_transparent;
      if (pixel[3] == 0 || (include_transparent && pixel[3] != 255)) {
        auto dist = std::numeric_limits<std::int64_t>::max();
        for (std::int64_t j = -std::int64_t(pixels); j <= pixels; ++j) {
          for (std::int64_t i = -std::int64_t(pixels); i <= pixels; ++i) {
            if (i + x >= pixels && j + y >= pixels && i + x < border_image_size[0] - pixels && j + y < border_image_size[1] - pixels) {
              Vec4B remote_pixel = image.get32(i + x - pixels, j + y - pixels);
              if (remote_pixel[3] != 0) {
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
            Color color = Color::rgba(op->get().start_color).mix(Color::rgba(op->get().end_color), percent);
            if (op->get().outline_only) {
              float pixel_a = byte_to_float(pixel[3]);
              color.set_alpha_f((1.0f - pixel_a) * std::fminf(pixel_a, 0.5f) * 2.0f);
            }
            else {
              Color pixel_f = Color::rgba(pixel);
              float pixel_a = pixel_f.alpha_f(), color_a = color.alpha_f();
              color_a += pixel_a * (1.0f - color_a);
              pixel_f.convert_to_linear(); //Mix in linear color space as it is more perceptually accurate
              color.convert_to_linear();
              color = color.mix(pixel_f, pixel_a);
              color.convert_to_srgb();
              color.set_alpha_f(color_a);
            }
            pixel = color.to_rgba();
          } else {
            pixel = Vec4B(Vec4F(op->get().start_color) * (1 - percent) + Vec4F(op->get().end_color) * percent);
          }
        }
      } else if (op->get().outline_only) {
        pixel = Vec4B(0, 0, 0, 0);
      }
    });

    image = std::move(border_image);

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
        for (std::size_t x_left = 0; x_left < image.width() / 2; ++x_left) {
          std::size_t x_right = image.width() - 1 - x_left;

          auto left = image.get32(x_left, y);
          auto right = image.get32(x_right, y);

          image.set32(x_left, y, right);
          image.set32(x_right, y, left);
        }
      }
    }

    if (op->get().mode == FlipImageOperation::FlipY || op->get().mode == FlipImageOperation::FlipXY) {
      for (std::size_t x = 0; x < image.width(); ++x) {
        for (std::size_t y_top = 0; y_top < image.height() / 2; ++y_top) {
          std::size_t y_bottom = image.height() - 1 - y_top;

          auto top = image.get32(x, y_top);
          auto bottom = image.get32(x, y_bottom);

          image.set32(x, y_top, bottom);
          image.set32(x, y_bottom, top);
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
          auto const& fade_tables = fade->get().color_tables();
          for (unsigned value = 0; value < 256; ++value) {
            tables[parity][0][value] = fade_tables.r_table[tables[parity][0][value]];
            tables[parity][1][value] = fade_tables.g_table[tables[parity][1][value]];
            tables[parity][2][value] = fade_tables.b_table[tables[parity][2][value]];
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
    auto data = image.span();
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
static std::expected<void, StarException> process_image_operation_sequence(std::size_t count, OperationAt operation_at,
                                                                           Image& image,
                                                                           ImageReferenceCallback const& ref_callback) {
  std::optional<std::reference_wrapper<ColorReplaceImageOperation const>> settled;
  PixelRect changed;
  bool saw_blend = false;
  for (std::size_t i = 0; i < count;) {
    auto const& operation = operation_at(i);

    if (std::holds_alternative<BlendImageOperation>(operation) && settled) {
      auto const& blend = std::get<BlendImageOperation>(operation);
      // Match process_image_operation's per-operation callback value semantics.
      auto operation_callback = ref_callback;
      auto blended = apply_blend(blend, image, operation_callback);
      if (!blended)
        return std::unexpected(std::move(blended.error()));
      changed.include(*blended);
      saw_blend = true;
      ++i;
      continue;
    }
    if (auto replace = (std::holds_alternative<ColorReplaceImageOperation>(operation)
                          ? std::optional{std::cref(std::get<ColorReplaceImageOperation>(operation))}
                          : std::nullopt);
        replace && settled && saw_blend
        && replace_keys_are_keys_of(replace->get().color_replace_map, settled->get().color_replace_map)
        && replace_settles(replace->get().color_replace_map)) {
      // Outside the consecutive blends' union there are no keys left to replace.
      replace_colors(replace->get().color_replace_map, image, changed);
      settled = replace;
      changed = {};
      saw_blend = false;
      ++i;
      continue;
    }
    settled.reset();
    changed = {};
    saw_blend = false;

    if (is_color_operation(operation)) {
      auto end = i + 1;
      bool can_reorder_crop = is_per_pixel_color_operation(operation);
      while (end < count && is_color_operation(operation_at(end))) {
        can_reorder_crop = can_reorder_crop && is_per_pixel_color_operation(operation_at(end));
        ++end;
      }
      if (can_reorder_crop && end < count) {
        if (auto crop = (std::holds_alternative<CropImageOperation>(operation_at(end))
                           ? std::optional{std::cref(std::get<CropImageOperation>(operation_at(end)))}
                           : std::nullopt);
            crop && crop_shrinks(crop->get(), image)) {
          auto cropped = process_image_operation(operation_at(end), image, ref_callback);
          if (!cropped)
            return std::unexpected(std::move(cropped.error()));
          auto processed = process_color_segment(i, end, operation_at, image, ref_callback);
          if (!processed)
            return std::unexpected(std::move(processed.error()));
          i = end + 1;
          continue;
        }
      }
      auto processed = process_color_segment(i, end, operation_at, image, ref_callback);
      if (!processed)
        return std::unexpected(std::move(processed.error()));
      // Only a successfully applied final replace establishes the settled invariant.
      if (image.bytes_per_pixel() == 4 && end < count && std::holds_alternative<BlendImageOperation>(operation_at(end))
          && std::holds_alternative<ColorReplaceImageOperation>(operation_at(end - 1))) {
        auto const& replace = std::get<ColorReplaceImageOperation>(operation_at(end - 1));
        if (replace_settles(replace.color_replace_map))
          settled = std::cref(replace);
      }
      i = end;
      continue;
    }

    if (std::holds_alternative<ScaleImageOperation>(operation)) {
      auto const& scale = std::get<ScaleImageOperation>(operation);
      auto end = i + 1;
      while (end < count && is_per_pixel_color_operation(operation_at(end)))
        ++end;
      if (end > i + 1 && scale_enlarges_by_copying(scale, image)) {
        auto processed = process_color_segment(i + 1, end, operation_at, image, ref_callback);
        if (!processed)
          return std::unexpected(std::move(processed.error()));
        if (end < count) {
          if (std::holds_alternative<CropImageOperation>(operation_at(end))) {
            auto const& crop = std::get<CropImageOperation>(operation_at(end));
            auto fused = process_scale_crop(scale, crop, image);
            if (!fused)
              return std::unexpected(std::move(fused.error()));
            if (*fused) {
              i = end + 1;
              continue;
            }
          }
        }
        auto scaled = process_image_operation(operation, image, ref_callback);
        if (!scaled)
          return std::unexpected(std::move(scaled.error()));
        i = end;
        continue;
      }
      if (i + 1 < count) {
        if (std::holds_alternative<CropImageOperation>(operation_at(i + 1))) {
          auto const& crop = std::get<CropImageOperation>(operation_at(i + 1));
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
    auto processed = process_image_operation(operation, image, ref_callback);
    if (!processed)
      return std::unexpected(std::move(processed.error()));
    ++i;
  }
  return {};
}

std::expected<void, StarException>
process_image_operations(std::span<std::reference_wrapper<ImageOperation const> const> operations, Image& image,
                         ImageReferenceCallback ref_callback) {
  return process_image_operation_sequence(
    operations.size(), [&](std::size_t i) -> ImageOperation const& { return operations[i].get(); }, image, ref_callback);
}

std::expected<Image, StarException> process_image_operations(List<ImageOperation> const& operations, Image image,
                                                             ImageReferenceCallback ref_callback) {
  auto processed = process_image_operation_sequence(
    operations.size(), [&](std::size_t i) -> ImageOperation const& { return operations[i]; }, image, ref_callback);
  if (!processed)
    return std::unexpected(std::move(processed.error()));
  return image;
}
}

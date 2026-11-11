module;
import std;
import star.format;
import star.list;
import star.rect;
import star.map;
import star.string;

import star.array;
import star.vector;
namespace Star {

class Json;
class StringView;
using JsonArray = List<Json>;

class Image;

struct ImageOperationExceptionTag {
  static constexpr std::string_view name() { return "ImageOperationException"; }
};
using ImageOperationException = StarError<ImageOperationExceptionTag, StarException>;

[[nodiscard]] std::expected<StringList, StarException> color_directives_from_config(JsonArray const& directives);
[[nodiscard]] std::expected<String, StarException> palette_swap_directives_from_config(Json const& swaps);

struct NullImageOperation {
  bool unloaded = false;
};

struct ErrorImageOperation {
  StarException cause;
  mutable bool reported = false;
};

struct HueShiftImageOperation {
  // Specify hue shift angle as -360 to 360 rather than -1 to 1
  static HueShiftImageOperation hue_shift_degrees(float degrees);

  // value here is normalized to 1.0
  float hueShiftAmount;
};

struct SaturationShiftImageOperation {
  // Specify saturation shift as amount normalized to 100
  static SaturationShiftImageOperation saturation_shift_100(float amount);

  // value here is normalized to 1.0
  float saturationShiftAmount;
};

struct BrightnessMultiplyImageOperation {
  // Specify brightness multiply as amount where 0 means "no change" and 100
  // means "x2" and -100 means "x0"
  static BrightnessMultiplyImageOperation brightness_multiply_100(float amount);

  float brightnessMultiply;
};

// Fades R G and B channels to the given color by the given amount, ignores A
struct FadeToColorImageOperation {
  FadeToColorImageOperation(Vec3B color, float amount);

  Vec3B color;
  float amount;

  Array<std::uint8_t, 256> rTable;
  Array<std::uint8_t, 256> gTable;
  Array<std::uint8_t, 256> bTable;
};

// Applies two FadeToColor operations in alternating rows to produce a scanline effect
struct ScanLinesImageOperation {
  FadeToColorImageOperation fade1;
  FadeToColorImageOperation fade2;
};

// Sets RGB values to the given color, and ignores the alpha channel
struct SetColorImageOperation {
  Vec3B color;
};

using ColorReplaceMap = HashMap<Vec4B, Vec4B>;

struct ColorReplaceImageOperation {
  ColorReplaceMap colorReplaceMap;
};

struct AlphaMaskImageOperation {
  enum MaskMode {
    Additive,
    Subtractive
  };

  MaskMode mode;
  StringList maskImages;
  Vec2I offset;
};

struct BlendImageOperation {
  enum BlendMode {
    Multiply,
    Screen
  };

  BlendMode mode;
  StringList blendImages;
  Vec2I offset;
};

struct MultiplyImageOperation {
  Vec4B color;
};

struct BorderImageOperation {
  unsigned pixels;
  Vec4B startColor;
  Vec4B endColor;
  bool outlineOnly;
  bool includeTransparent;
};

struct ScaleImageOperation {
  enum Mode {
    Nearest,
    Bilinear,
    Bicubic
  };

  Mode mode;
  Vec2F scale;
};

struct CropImageOperation {
  RectI subset;
};

struct FlipImageOperation {
  enum Mode {
    FlipX,
    FlipY,
    FlipXY
  };
  Mode mode;
};

using ImageOperation =
  std::variant<NullImageOperation, ErrorImageOperation, HueShiftImageOperation, SaturationShiftImageOperation,
               BrightnessMultiplyImageOperation, FadeToColorImageOperation, ScanLinesImageOperation,
               SetColorImageOperation, ColorReplaceImageOperation, AlphaMaskImageOperation, BlendImageOperation,
               MultiplyImageOperation, BorderImageOperation, ScaleImageOperation, CropImageOperation,
               FlipImageOperation>;

ImageOperation image_operation_from_string(StringView string);
String image_operation_to_string(ImageOperation const& operation);

[[nodiscard]] std::expected<void, StarException>
parse_image_operations(StringView params,
                       std::function<std::expected<void, StarException>(ImageOperation&&)> outputter);

// Each operation is assumed to be separated by '?', with parameters
// separated by ';' or '='
List<ImageOperation> parse_image_operations(StringView params);

// Each operation separated by '?', returns string with leading '?'
String print_image_operations(List<ImageOperation> const& operations);

void add_image_operation_references(ImageOperation const& operation, StringList& out);

StringList image_operation_references(List<ImageOperation> const& operations);

using ImageReferenceCallback =
  std::function<std::expected<std::optional<std::reference_wrapper<Image const>>, StarException>(
    String const& refName)>;

[[nodiscard]] std::expected<void, StarException> process_image_operation(ImageOperation const& operation, Image& input,
                                                                         ImageReferenceCallback refCallback = {});

[[nodiscard]] std::expected<Image, StarException>
process_image_operations(List<ImageOperation> const& operations, Image input, ImageReferenceCallback refCallback = {});

// Applies the same optimizer in place to stable operation references. On failure,
// successful prefix mutations and the failing operation's format promotion remain.
[[nodiscard]] std::expected<void, StarException>
process_image_operations(std::span<std::reference_wrapper<ImageOperation const> const> operations, Image& image,
                         ImageReferenceCallback refCallback = {});
}

export module star.image_processing;

export namespace Star {
using ::Star::add_image_operation_references;
using ::Star::AlphaMaskImageOperation;
using ::Star::BlendImageOperation;
using ::Star::BorderImageOperation;
using ::Star::BrightnessMultiplyImageOperation;
using ::Star::color_directives_from_config;
using ::Star::ColorReplaceImageOperation;
using ::Star::ColorReplaceMap;
using ::Star::CropImageOperation;
using ::Star::ErrorImageOperation;
using ::Star::FadeToColorImageOperation;
using ::Star::FlipImageOperation;
using ::Star::HueShiftImageOperation;
using ::Star::Image;
using ::Star::image_operation_from_string;
using ::Star::image_operation_references;
using ::Star::image_operation_to_string;
using ::Star::ImageOperation;
using ::Star::ImageOperationException;
using ::Star::ImageOperationExceptionTag;
using ::Star::ImageReferenceCallback;
using ::Star::MultiplyImageOperation;
using ::Star::NullImageOperation;
using ::Star::palette_swap_directives_from_config;
using ::Star::parse_image_operations;
using ::Star::print_image_operations;
using ::Star::process_image_operation;
using ::Star::process_image_operations;
using ::Star::SaturationShiftImageOperation;
using ::Star::ScaleImageOperation;
using ::Star::ScanLinesImageOperation;
using ::Star::SetColorImageOperation;
}

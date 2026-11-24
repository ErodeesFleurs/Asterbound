import star.format;
import star.json;
import star.rect;
import star.thread;
import star.list;
import star.image_processing;
import star.image_scaling;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.random;
import star.ttl_cache;
import star.file;
import star.image;
import star.logging;
import star.string;
import star.byte_array;
import star.encode;
import star.vector;
import star.game_types;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.image_metadata_database;

import star.math_common;
namespace Star {

namespace {
constexpr std::chrono::seconds cache_lifetime{60};
constexpr std::chrono::seconds cache_smear{2};
}

ImageMetadataDatabase::ImageMetadataDatabase(Root& root)
  : m_root(root),
    m_size_cache(root.presentationRandom(), cache_lifetime, cache_smear),
    m_spaces_cache(root.presentationRandom(), cache_lifetime, cache_smear),
    m_region_cache(root.presentationRandom(), cache_lifetime, cache_smear) {}

std::expected<Vec2U, StarException> ImageMetadataDatabase::image_size(AssetPath const& path) const {
  auto layout = cached_image_layout(path);
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  return layout->first;
}

auto ImageMetadataDatabase::cached_image_layout(AssetPath const& path) const -> std::expected<ImageLayout, StarException> {
  std::unique_lock locker(m_mutex);
  auto cached = m_size_cache.find_ref(path);
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  if (*cached)
    return (*cached)->get();
  locker.unlock();
  auto size = calculate_image_size(path);
  if (!size)
    return std::unexpected(std::move(size.error()));
  locker.lock();
  auto stored = m_size_cache.insert_or_assign(path, *size);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  return *size;
}

namespace {
std::expected<std::shared_ptr<Image const>, StarException> load_geometry_image(
    Assets const& assets, AssetPath const& path, std::optional<AssetPath>& filtered) {
  if (filtered) {
    auto image = assets.image(*filtered);
    if (image && ((*image)->bytes_per_pixel() == 3 || (*image)->bytes_per_pixel() == 4))
      return image;
    // Keep the original directive execution and diagnostics on other formats.
    filtered.reset();
  }
  return assets.image(path);
}

bool finite_occupancy_input(float const& value) {
  // xclang -ffast-math folds the nonvolatile exponent check to true.
  volatile auto bits = std::bit_cast<std::uint32_t>(value);
  return (bits & 0x7f800000u) != 0x7f800000u;
}

float occupancy_key_value(float value) {
  return (std::bit_cast<std::uint32_t>(value) << 1) == 0 ? 0.0f : value;
}

struct TileSamples {
  std::array<unsigned, tile_pixels> pixels{};
  unsigned count = 0;
};

TileSamples sample_axis(std::int64_t tile, float position, unsigned extent, bool flip) {
  TileSamples samples;
  for (int offset = 0; offset < static_cast<int>(tile_pixels); ++offset) {
    float pixel = std::round(static_cast<float>(tile * static_cast<int>(tile_pixels) + offset) - position);
    if (pixel >= 0 && static_cast<double>(pixel) < extent) {
      auto index = static_cast<unsigned>(pixel);
      samples.pixels[samples.count++] = flip ? extent - 1 - index : index;
    }
  }
  return samples;
}

std::expected<List<Vec2I>, StarException> scan_spaces(Image const& image, Vec2F position, float fill_limit, bool flip) {
  std::array<std::int64_t, 2> begin, end;
  auto dimensions = image.size();
  for (unsigned axis = 0; axis < 2; ++axis) {
    double first = std::floor(position[axis] / tile_pixels);
    double last = std::ceil((static_cast<float>(dimensions[axis]) + position[axis]) / tile_pixels);
    if (last > static_cast<double>(std::numeric_limits<int>::max()) + 1)
      return std::unexpected(StarException("Image occupancy exceeds tile coordinate range"));
    begin[axis] = static_cast<std::int64_t>(first);
    end[axis] = static_cast<std::int64_t>(last);
  }
  List<Vec2I> spaces;
  if (fill_limit > 1)
    return spaces;
  auto const required = fill_limit > 0 ? static_cast<unsigned>(std::ceil(fill_limit * (tile_pixels * tile_pixels))) : 0u;
  auto const stride = image.bytes_per_pixel();
  auto const data = image.span();
  for (auto y = begin[1]; y < end[1]; ++y) {
    auto rows = required ? sample_axis(y, position[1], dimensions[1], false) : TileSamples{};
    for (auto x = begin[0]; x < end[0]; ++x) {
      if (required) {
        auto columns = sample_axis(x, position[0], dimensions[0], flip);
        if (rows.count * columns.count < required)
          continue;
        if (stride != 3) {
          unsigned count = 0;
          for (unsigned j = 0; j < rows.count && count < required; ++j) {
            auto row = rows.pixels[j];
            auto offset = static_cast<std::size_t>(row) * dimensions[0];
            for (unsigned i = 0; i < columns.count; ++i) {
              auto column = columns.pixels[i];
              if (stride == 4) {
                count += data[(offset + column) * 4 + 3] != 0;
              } else {
                auto pixel = image.get(column, row);
                if (!pixel)
                  return std::unexpected(std::move(pixel.error()));
                count += (*pixel)[3] != 0;
              }
            }
          }
          if (count < required)
            continue;
        }
      }
      spaces.emplace_back(static_cast<int>(x), static_cast<int>(y));
    }
  }
  return spaces;
}
}

std::expected<List<Vec2I>, StarException> ImageMetadataDatabase::image_spaces(AssetPath const& path, Vec2F position, float fill_limit, bool flip) const {
  if (!finite_occupancy_input(position[0]) || !finite_occupancy_input(position[1]) || !finite_occupancy_input(fill_limit))
    return std::unexpected(StarException("Image occupancy requires finite position and fill limit"));
  constexpr double min_position = static_cast<double>(std::numeric_limits<int>::min()) * tile_pixels;
  constexpr double end_position = (static_cast<double>(std::numeric_limits<int>::max()) + 1) * tile_pixels;
  if (position[0] < min_position || position[0] >= end_position || position[1] < min_position || position[1] >= end_position)
    return std::unexpected(StarException("Image occupancy exceeds tile coordinate range"));
  position = {occupancy_key_value(position[0]), occupancy_key_value(position[1])};
  fill_limit = occupancy_key_value(fill_limit);
  SpacesEntry key(path, position, fill_limit, flip);
  {
    std::lock_guard locker(m_mutex);
    auto cached = m_spaces_cache.find_ref(key);
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    if (*cached)
      return (*cached)->get();
  }
  auto filtered = filter_processing(path);
  std::optional<SpacesEntry> filtered_key;
  if (filtered) {
    filtered_key.emplace(*filtered, position, fill_limit, flip);
    std::lock_guard locker(m_mutex);
    auto cached = m_spaces_cache.find_ref(*filtered_key);
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    if (*cached) {
      List<Vec2I> spaces = (*cached)->get();
      auto stored = m_spaces_cache.insert_or_assign(std::move(key), spaces);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      return spaces;
    }
  }
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto image = load_geometry_image(**assets, path, filtered);
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto spaces = scan_spaces(**image, position, fill_limit, flip);
  if (!spaces || ((*image)->bytes_per_pixel() != 3 && (*image)->bytes_per_pixel() != 4))
    return spaces;
  std::lock_guard locker(m_mutex);
  auto stored = m_spaces_cache.insert_or_assign(std::move(key), *spaces);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  if (filtered) {
    auto alias = m_spaces_cache.insert_or_assign(std::move(*filtered_key), *spaces);
    if (!alias)
      return std::unexpected(std::move(alias.error()));
  }
  return spaces;
}

std::expected<RectU, StarException> ImageMetadataDatabase::non_empty_region(AssetPath const& path) const {
  {
    std::lock_guard locker(m_mutex);
    auto cached = m_region_cache.find_ref(path);
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    if (*cached)
      return (*cached)->get();
  }

  auto filtered = filter_processing(path);
  if (filtered) {
    std::lock_guard locker(m_mutex);
    auto cached = m_region_cache.find_ref(*filtered);
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    if (*cached) {
      auto region = (*cached)->get();
      auto stored = m_region_cache.insert_or_assign(path, region);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      return region;
    }
  }

  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto image = load_geometry_image(**assets, path, filtered);
  if (!image)
    return std::unexpected(std::move(image.error()));
  RectU region = RectU::null();
  auto const& pixels = **image;
  auto const bytes_per_pixel = pixels.bytes_per_pixel();
  if (bytes_per_pixel == 3) {
    if (pixels.width() != 0 && pixels.height() != 0)
      region = RectU::with_size(Vec2U(), pixels.size());
  } else if (bytes_per_pixel == 4) {
    auto const image_data = pixels.span();
    unsigned min_x = pixels.width(), min_y = pixels.height();
    unsigned max_x = 0, max_y = 0;
    auto const stride = std::size_t(pixels.width()) * 4;
    for (unsigned y = 0; y < pixels.height(); ++y) {
      auto row = image_data.subspan(std::size_t(y) * stride, stride);
      unsigned left = 0;
      while (left < pixels.width() && row[std::size_t(left) * 4 + 3] == 0)
        ++left;
      if (left == pixels.width())
        continue;
      unsigned right = pixels.width();
      while (right > left + 1 && row[std::size_t(right - 1) * 4 + 3] == 0)
        --right;
      min_x = std::min(min_x, left);
      max_x = std::max(max_x, right);
      min_y = std::min(min_y, y);
      max_y = y + 1;
    }
    if (min_x < pixels.width())
      region = RectU(Vec2U(min_x, min_y), Vec2U(max_x, max_y));
  } else {
    // Float-format traversal deliberately retains its legacy implicit alpha.
    pixels.for_each_pixel([&region](unsigned x, unsigned y, Vec4B const& pixel) {
      if (pixel[3] > 0)
        region.combine(RectU::with_size({x, y}, {1, 1}));
    });
    return region; // Only byte-format results may serve normalized lookups.
  }

  std::lock_guard locker(m_mutex);
  auto stored = m_region_cache.insert_or_assign(path, region);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  if (filtered) {
    auto alias = m_region_cache.insert_or_assign(std::move(*filtered), region);
    if (!alias)
      return std::unexpected(std::move(alias.error()));
  }

  return region;
}

void ImageMetadataDatabase::cleanup() const {
  std::lock_guard locker(m_mutex);

  m_size_cache.cleanup();
  m_spaces_cache.cleanup();
  m_region_cache.cleanup();
}

std::optional<AssetPath> ImageMetadataDatabase::filter_processing(AssetPath const& path) {
  auto const& groups = path.directives.list();
  auto group_count = groups.size();
  std::size_t prefix_size = 0;
  while (group_count) {
    auto const& group = groups[group_count - 1];
    auto entries = group.entries();
    auto kept = entries.size();
    while (kept) {
      auto const& entry = entries[kept - 1];
      // Reject non-candidates without forcing a long group's lazy parse.
      auto text = group.entry_string(entry).utf8();
      auto name = text.substr(0, text.find_first_of("=;"));
      if (name != "hueshift" && name != "saturation" && name != "brightness" && name != "fade"
          && name != "scanlines" && name != "setcolor")
        break;
      if (std::holds_alternative<ErrorImageOperation>(group.entry_operation(entry)))
        break;
      --kept;
    }
    if (kept) {
      if (kept != entries.size())
        prefix_size = entries[kept].begin - 1; // exclude the removed operation's '?'
      else if (group_count == groups.size())
        return {};
      break;
    }
    --group_count;
  }
  if (groups.empty())
    return {};

  // Earlier colors can feed a later replace. Retain all prefix operations,
  // including group boundaries (a group's bare prefix is not a '?directive').
  AssetPath filtered{path.base_path, path.sub_path, {}};
  for (std::size_t i = 0; i < group_count - (prefix_size != 0); ++i)
    filtered.directives.append(groups[i]);
  if (prefix_size) {
    auto const& text = groups[group_count - 1].maybe_string()->get().utf8();
    filtered.directives.append(Directives(std::string_view(text).substr(0, prefix_size)));
  }
  return filtered;
}

namespace {
template <typename T>
concept size_neutral_operation =
    std::same_as<T, NullImageOperation> || std::same_as<T, ErrorImageOperation> || std::same_as<T, HueShiftImageOperation>
    || std::same_as<T, SaturationShiftImageOperation> || std::same_as<T, BrightnessMultiplyImageOperation>
    || std::same_as<T, FadeToColorImageOperation> || std::same_as<T, ScanLinesImageOperation>
    || std::same_as<T, SetColorImageOperation> || std::same_as<T, ColorReplaceImageOperation>
    || std::same_as<T, AlphaMaskImageOperation> || std::same_as<T, BlendImageOperation>
    || std::same_as<T, MultiplyImageOperation> || std::same_as<T, FlipImageOperation>;

struct ImageLayoutAdjust {
  Vec2U size;
  PixelFormat format;
  bool needs_crop = false;

  bool operator()(size_neutral_operation auto const&) const { return !needs_crop; }

  bool operator()(BorderImageOperation const& border) {
    if (needs_crop)
      return false;
    auto result = bordered_image_size(size, border.pixels);
    if (!result)
      return false;
    size = *result;
    format = PixelFormat::RGBA32;
    return true;
  }

  bool operator()(ScaleImageOperation const& scale) {
    if (needs_crop)
      return false;
    auto result = scaled_image_size(size, scale.scale);
    if (!result)
      return false;
    size = *result;
    // A fused byte-format scale/crop needs storage for the window only.
    needs_crop = !Image::size_bytes(size, format);
    return !needs_crop || (bytes_per_pixel(format) == 4 && scale.mode != ScaleImageOperation::Bicubic);
  }

  bool operator()(CropImageOperation const& crop) {
    auto min = crop.subset.min(), max = crop.subset.max();
    for (unsigned axis = 0; axis < 2; ++axis)
      if (min[axis] < 0 || max[axis] < min[axis] || static_cast<unsigned>(max[axis]) > size[axis])
        return false;
    size = Vec2U(crop.subset.size());
    needs_crop = false;
    return Image::size_bytes(size, format).has_value();
  }
};
}

auto ImageMetadataDatabase::calculate_image_size(AssetPath const& path) const -> std::expected<ImageLayout, StarException> {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto fallback = [&]() -> std::expected<ImageLayout, StarException> {
    auto image = (*assets)->image(path);
    if (!image)
      return std::unexpected(std::move(image.error()));
    return ImageLayout{(*image)->size(), (*image)->pixel_format()};
  };
  if (!path.sub_path && path.directives.empty()) {
    auto hint = (*assets)->image_size_hint(path.base_path);
    if (!hint)
      return std::unexpected(std::move(hint.error()));
    if (*hint)
      return **hint;
    return fallback();
  }

  auto base = cached_image_layout({path.base_path, {}, {}});
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto [size, format] = *base;
  if (path.sub_path) {
    auto frames = (*assets)->image_frames(path.base_path);
    if (!frames)
      return std::unexpected(std::move(frames.error()));
    if (!*frames)
      return fallback();
    auto rect = (*frames)->get_rect(*path.sub_path);
    if (!rect)
      return std::unexpected(std::move(rect.error()));
    if (!*rect)
      return fallback();
    auto const& frame = **rect;
    if (frame.x_max() < frame.x_min() || frame.y_max() < frame.y_min()
        || frame.x_max() > size[0] || frame.y_max() > size[1])
      return fallback();
    size = frame.size();
  }
  if (path.directives.empty())
    return ImageLayout{size, format};
  if (format == PixelFormat::RGB24)
    format = PixelFormat::RGBA32;
  else if (format == PixelFormat::BGR24)
    format = PixelFormat::BGRA32;
  ImageLayoutAdjust adjust{size, format};
  for (auto const& directives : path.directives.list())
    for (auto const& operation : directives.operations())
      if (!std::visit(adjust, operation))
        return fallback();
  if (adjust.needs_crop)
    return fallback();
  return ImageLayout{adjust.size, adjust.format};
}

}

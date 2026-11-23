#include <vulkan/vulkan.h>
#include <shaderc/shaderc.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "imgui.h"
#include "imgui_impl_vulkan.h"
import std;
import star.renderer_vulkan;
import star.renderer;
import star.logging;
import star.format;
import star.string;
import star.list;
import star.set;
import star.rect;
import star.json;
import star.json_extra;
import star.image;
import star.texture_atlas;
import star.casting;
import star.ref_ptr;
import star.type_identity;
import star.vector;
import star.matrix3;
import star.math_common;
import star.bi_map;

namespace Star {
namespace {
template <typename T> using Result = std::expected<T, StarException>;
std::unexpected<StarException> vkError(std::string_view operation, VkResult result) {
  return std::unexpected<StarException>(RendererException::format("Vulkan {} failed ({})", operation, static_cast<int>(result)));
}
#define VK_REQUIRE(expression) do { auto vkResult = (expression); if (vkResult != VK_SUCCESS) return vkError(#expression, vkResult); } while (false)
#define RENDER_REQUIRE(expression) do { auto operationResult = (expression); if (!operationResult) return std::unexpected(std::move(operationResult.error())); } while (false)

struct Device {
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice logical = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t family = 0;
  VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceMemoryProperties memory{};
  bool sampleShading = false;
  bool portability = false;
  bool swapchainColorspace = false;
  bool hdrMetadata = false;
  ~Device() {
    if (logical) { vkDeviceWaitIdle(logical); vkDestroyDevice(logical, nullptr); }
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
  }
  Result<std::uint32_t> memoryType(std::uint32_t mask, VkMemoryPropertyFlags flags) const {
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i)
      if ((mask & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    return std::unexpected(RendererException("Vulkan has no compatible memory type"));
  }
};
struct Buffer {
  std::shared_ptr<Device> device;
  VkBuffer handle = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  // Mapped memory is an unavoidable Vulkan C ABI boundary, never a borrowed API.
  void* mapped = nullptr;
  ~Buffer() {
    if (mapped) vkUnmapMemory(device->logical, memory);
    if (handle) vkDestroyBuffer(device->logical, handle, nullptr);
    if (memory) vkFreeMemory(device->logical, memory, nullptr);
  }
};
Result<std::shared_ptr<Buffer>> makeBuffer(std::shared_ptr<Device> const& device, VkDeviceSize size, VkBufferUsageFlags usage) {
  auto buffer = std::make_shared<Buffer>();
  buffer->device = device; buffer->size = std::max<VkDeviceSize>(size, 16);
  VkBufferCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.size = buffer->size; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VK_REQUIRE(vkCreateBuffer(device->logical, &info, nullptr, &buffer->handle));
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(device->logical, buffer->handle, &requirements);
  auto type = device->memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (!type) return std::unexpected(std::move(type.error()));
  VkMemoryAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = *type;
  VK_REQUIRE(vkAllocateMemory(device->logical, &allocation, nullptr, &buffer->memory));
  VK_REQUIRE(vkBindBufferMemory(device->logical, buffer->handle, buffer->memory, 0));
  VK_REQUIRE(vkMapMemory(device->logical, buffer->memory, 0, buffer->size, 0, &buffer->mapped));
  return buffer;
}
struct Upload {
  Vec2U offset;
  Vec2U size;
  std::vector<std::uint8_t> bytes;
};
struct GpuImage {
  std::shared_ptr<Device> device;
  VkImage handle = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkImageView sampledView = VK_NULL_HANDLE;
  VkImageView srgbView = VK_NULL_HANDLE;
  bool opaque = false;
  VkSampler sampler = VK_NULL_HANDLE;
  Vec2U size;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  TextureFiltering filtering = TextureFiltering::Nearest;
  TextureAddressing addressing = TextureAddressing::Clamp;
  std::vector<Upload> pending;
  ~GpuImage() {
    if (sampler) vkDestroySampler(device->logical, sampler, nullptr);
    if (sampledView) vkDestroyImageView(device->logical, sampledView, nullptr);
    if (srgbView) vkDestroyImageView(device->logical, srgbView, nullptr);
    if (view) vkDestroyImageView(device->logical, view, nullptr);
    if (handle) vkDestroyImage(device->logical, handle, nullptr);
    if (memory) vkFreeMemory(device->logical, memory, nullptr);
  }
};
Result<std::shared_ptr<GpuImage>> makeImage(std::shared_ptr<Device> const& device, Vec2U size, VkFormat format,
    VkSampleCountFlagBits samples, TextureAddressing addressing, TextureFiltering filtering) {
  if (!size[0] || !size[1] || size[0] > device->properties.limits.maxImageDimension2D || size[1] > device->properties.limits.maxImageDimension2D)
    return std::unexpected(RendererException("Vulkan image dimensions are zero or exceed device limits"));
  auto flags = format == VK_FORMAT_R8G8B8A8_UNORM ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : VkImageCreateFlags(0);
  VkImageFormatProperties supported{};
  auto support = vkGetPhysicalDeviceImageFormatProperties(device->physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, flags, &supported);
  if (support != VK_SUCCESS || !(supported.sampleCounts & samples))
    return std::unexpected(RendererException::format("Vulkan format {} does not support {} samples", static_cast<int>(format), static_cast<unsigned>(samples)));
  VkFormatProperties features;
  vkGetPhysicalDeviceFormatProperties(device->physical, format, &features);
  if (filtering == TextureFiltering::Linear && !(features.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    return std::unexpected(RendererException::format("Vulkan format {} does not support linear texture filtering", static_cast<int>(format)));
  auto image = std::make_shared<GpuImage>();
  image->device = device; image->size = size; image->format = format; image->samples = samples;
  image->addressing = addressing; image->filtering = filtering;
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.flags = flags;
  info.imageType = VK_IMAGE_TYPE_2D; info.format = format; info.extent = {size[0], size[1], 1};
  info.mipLevels = 1; info.arrayLayers = 1; info.samples = samples; info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  VK_REQUIRE(vkCreateImage(device->logical, &info, nullptr, &image->handle));
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device->logical, image->handle, &requirements);
  auto type = device->memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (!type) return std::unexpected(std::move(type.error()));
  VkMemoryAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = *type;
  VK_REQUIRE(vkAllocateMemory(device->logical, &allocation, nullptr, &image->memory));
  VK_REQUIRE(vkBindImageMemory(device->logical, image->handle, image->memory, 0));
  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.image = image->handle; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = format;
  view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VK_REQUIRE(vkCreateImageView(device->logical, &view, nullptr, &image->view));
  VkSamplerCreateInfo sampler{};
  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.magFilter = sampler.minFilter = filtering == TextureFiltering::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = addressing == TextureAddressing::Wrap ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = 0;
  VK_REQUIRE(vkCreateSampler(device->logical, &sampler, nullptr, &image->sampler));
  return image;
}
Result<void> copyPixels(ImageView const& source, std::span<std::uint8_t> destination) {
  bool floating = source.format == PixelFormat::RGB_F || source.format == PixelFormat::RGBA_F;
  bool rgb = source.format == PixelFormat::RGB24 || source.format == PixelFormat::BGR24 || source.format == PixelFormat::RGB_F;
  bool bgr = source.format == PixelFormat::BGR24 || source.format == PixelFormat::BGRA32;
  auto count = std::size_t(source.size[0]) * source.size[1];
  auto sourceStride = bytes_per_pixel(source.format);
  if (count > std::numeric_limits<std::size_t>::max() / (floating ? 16 : 4)
      || source.data.size() != count * sourceStride || destination.size() != count * (floating ? 16 : 4))
    return std::unexpected(RendererException("Invalid Vulkan texture image byte count"));
  if ((!rgb && !bgr) || (floating && !rgb)) {
    std::memcpy(destination.data(), source.data.data(), destination.size());
    return {};
  }
  for (std::size_t i = 0; i < count; ++i) {
    if (floating) {
      std::array<float, 4> pixel{0, 0, 0, 1};
      std::memcpy(pixel.data(), source.data.data() + i * sourceStride, sourceStride);
      std::memcpy(destination.data() + i * 16, pixel.data(), 16);
    } else {
      auto input = source.data.subspan(i * sourceStride, sourceStride);
      auto output = destination.subspan(i * 4, 4);
      output[0] = input[bgr ? 2 : 0]; output[1] = input[1]; output[2] = input[bgr ? 0 : 2]; output[3] = rgb ? 255 : input[3];
    }
  }
  return {};
}
Result<Upload> packPixels(ImageView const& source, Vec2U offset = Vec2U(0, 0)) {
  auto count = std::size_t(source.size[0]) * source.size[1];
  auto stride = source.format == PixelFormat::RGB_F || source.format == PixelFormat::RGBA_F ? 16u : 4u;
  if (count > std::numeric_limits<std::size_t>::max() / stride)
    return std::unexpected(RendererException("Vulkan texture byte count overflow"));
  Upload upload{offset, source.size, std::vector<std::uint8_t>(count * stride)};
  RENDER_REQUIRE(copyPixels(source, upload.bytes));
  return upload;
}
Result<std::shared_ptr<GpuImage>> imageFromPixels(std::shared_ptr<Device> const& device, ImageView const& pixels,
    TextureAddressing addressing, TextureFiltering filtering) {
  Vec2U size = pixels.empty() ? Vec2U(1, 1) : pixels.size;
  auto format = pixels.format == PixelFormat::RGB_F || pixels.format == PixelFormat::RGBA_F ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
  auto image = makeImage(device, size, format, VK_SAMPLE_COUNT_1_BIT, addressing, filtering);
  if (!image) return std::unexpected(std::move(image.error()));
  if (!pixels.empty()) {
    auto upload = packPixels(pixels);
    if (!upload) return std::unexpected(std::move(upload.error()));
    (*image)->pending.push_back(std::move(*upload));
  } else {
    (*image)->pending.push_back({Vec2U(0, 0), size, std::vector<std::uint8_t>(format == VK_FORMAT_R32G32B32A32_SFLOAT ? 16 : 4, 0)});
  }
  return image;
}
struct Atlas;
struct VkTexture : Texture {
  void query_type(TypeQuery& query) const noexcept override { query_type_of<VkTexture, Texture>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<VkTexture>(); }
  Vec2U dimensions;
  Vec2U offset{0, 0};
  std::shared_ptr<GpuImage> image;
  // The atlas allocation is released only after retained geometry and GPU frames
  // release this texture. Coordinates cannot move while packed vertices exist.
  std::shared_ptr<Atlas> atlas;
  TextureAtlasSet<std::shared_ptr<GpuImage>>::TextureHandle allocation;
  ~VkTexture() override;
  Vec2U size() const override { return dimensions; }
  TextureFiltering filtering() const override { return image->filtering; }
  TextureAddressing addressing() const override { return image->addressing; }
};
struct Atlas final : TextureAtlasSet<std::shared_ptr<GpuImage>> {
  Atlas(std::shared_ptr<Device> owner, unsigned cells, TextureFiltering filtering)
    : TextureAtlasSet(16, cells), device(std::move(owner)), filter(filtering) {}
  ~Atlas() override { reset(); }
  std::shared_ptr<Device> device;
  TextureFiltering filter;
  Result<std::shared_ptr<GpuImage>> createAtlasTexture(Vec2U const& size, PixelFormat format) override {
    auto gpuFormat = format == PixelFormat::RGB_F || format == PixelFormat::RGBA_F ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
    return makeImage(device, size, gpuFormat, VK_SAMPLE_COUNT_1_BIT, TextureAddressing::Clamp, filter);
  }
  // The atlas container erases its owning handle after this callback. The
  // image's destructor releases Vulkan storage once GPU frame pins retire.
  void destroyAtlasTexture(std::shared_ptr<GpuImage> const&) override {}
  Result<void> copyAtlasPixels(std::shared_ptr<GpuImage> const& image, Vec2U const& offset, Image const& pixels) override {
    auto upload = packPixels(ImageView(pixels), offset);
    if (!upload) return std::unexpected(std::move(upload.error()));
    image->pending.push_back(std::move(*upload));
    return {};
  }
};
VkTexture::~VkTexture() {
  if (allocation) {
    // Allocations have a one-pixel border and are locked against compaction.
    // Cancel an unsubmitted upload before freeing its cells for another texture;
    // otherwise reused cells can receive overlapping, unordered transfer writes.
    // Draw/frame texture pins keep submitted allocations alive until completion.
    auto uploadOffset = offset - Vec2U(1, 1);
    std::erase_if(image->pending, [&](Upload const& upload) { return upload.offset == uploadOffset; });
    auto freed = atlas->freeTexture(allocation);
    if (!freed) EmergencyLog::error("Vulkan atlas release: {}", freed.error().what());
  }
}
struct VkTextureGroup final : TextureGroup {
  std::shared_ptr<Atlas> atlas;
  TextureFiltering filtering() const override { return atlas->filter; }
  Result<TexturePtr> create(Image const& pixels) override {
    auto texture = make_ref<VkTexture>();
    texture->dimensions = pixels.size();
    auto size = atlas->atlasTextureSize();
    if (pixels.empty() || pixels.width() + 2 > size[0] || pixels.height() + 2 > size[1]) {
      auto image = imageFromPixels(atlas->device, ImageView(pixels), TextureAddressing::Clamp, filtering());
      if (!image) return std::unexpected(std::move(image.error()));
      texture->image = std::move(*image);
    } else {
      auto allocation = atlas->addTexture(pixels);
      if (!allocation) return std::unexpected(std::move(allocation.error()));
      (*allocation)->setLocked(true);
      texture->image = (*allocation)->atlasTexture();
      texture->offset = (*allocation)->atlasTextureCoordinates().min();
      texture->atlas = atlas;
      texture->allocation = std::move(*allocation);
    }
    return TexturePtr(std::move(texture));
  }
};
struct Vertex { Vec2F position; Vec2F uv; Vec4B color; std::uint32_t flags; };
struct Batch {
  std::array<std::shared_ptr<GpuImage>, 4> images;
  std::size_t textureCount = 0;
  std::uint32_t first = 0, count = 0;
};
struct Geometry {
  std::vector<Vertex> vertices;
  std::vector<Batch> batches;
  HashSet<TexturePtr> pins;
  std::shared_ptr<Buffer> buffer;
  bool dirty = true;
};
struct VkRenderBuffer final : RenderBuffer {
  void query_type(TypeQuery& query) const noexcept override { query_type_of<VkRenderBuffer, RenderBuffer>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<VkRenderBuffer>(); }
  std::shared_ptr<Device> device;
  TexturePtr white;
  bool multi = true;
  std::shared_ptr<Geometry> geometry = std::make_shared<Geometry>();
  std::vector<std::shared_ptr<Geometry>> reusable;
  template <typename Input> Result<void> replace(Input const& input) {
    std::shared_ptr<Geometry> replacement;
    auto available = std::find_if(reusable.begin(), reusable.end(), [](auto const& value) { return value.use_count() == 1; });
    if (available != reusable.end()) {
      replacement = std::move(*available);
      reusable.erase(available);
      replacement->vertices.clear(); replacement->batches.clear(); replacement->pins.clear();
    } else {
      replacement = std::make_shared<Geometry>();
    }
    replacement->dirty = true;
    auto previous = std::exchange(geometry, std::move(replacement));
    auto result = append(input);
    if (!result) {
      reusable.push_back(std::exchange(geometry, std::move(previous)));
      return std::unexpected(std::move(result.error()));
    }
    // Submitted draws own image/texture pins independently. Pool only CPU
    // capacity and the buffer generation, which stays fenced by its frames.
    previous->vertices.clear(); previous->batches.clear(); previous->pins.clear();
    reusable.push_back(std::move(previous));
    return {};
  }
  template <typename At> Result<void> append(std::uint8_t kind, std::size_t count, TexturePtr const& input, At const& at) {
    if (kind == 2 && count < 3) return {};
    auto texture = input ? input : white;
    auto typed = as_ref<VkTexture>(*texture);
    if (!typed || typed->get().image->device != device)
      return std::unexpected(RendererException("Vulkan primitive uses a texture from another renderer"));
    auto& g = *geometry;
    if (g.batches.empty()) g.batches.emplace_back();
    auto indexOf = [&](Batch const& batch) {
      std::size_t i = 0;
      while (i < batch.textureCount && batch.images[i] != typed->get().image) ++i;
      return i;
    };
    auto index = indexOf(g.batches.back());
    if (index == g.batches.back().textureCount && index >= (multi ? 4u : 1u)) {
      g.batches.emplace_back(); g.batches.back().first = g.vertices.size(); index = 0;
    }
    auto outputCount = kind == 0 ? 3 : kind == 1 ? 6 : (count - 2) * 3;
    if (outputCount > std::numeric_limits<std::uint32_t>::max() - g.vertices.size())
      return std::unexpected(RendererException("Vulkan geometry exceeds the draw vertex range"));
    auto& batch = g.batches.back();
    if (index == batch.textureCount) batch.images[batch.textureCount++] = typed->get().image;
    g.pins.add(texture);
    auto vertex = [&](std::size_t i, std::size_t previous, std::size_t next) {
      auto const& v = at(i);
      auto rX = std::min(std::abs(v.screenCoordinate[0] - at(previous).screenCoordinate[0]), std::abs(v.screenCoordinate[0] - at(next).screenCoordinate[0])) < 0.001f;
      auto rY = std::min(std::abs(v.screenCoordinate[1] - at(previous).screenCoordinate[1]), std::abs(v.screenCoordinate[1] - at(next).screenCoordinate[1])) < 0.001f;
      return Vertex{v.screenCoordinate, v.textureCoordinate + Vec2F(typed->get().offset), v.color,
          static_cast<std::uint32_t>(index) | (std::uint32_t(v.param1 > 0) << 2) | (std::uint32_t(rX) << 3) | (std::uint32_t(rY) << 4)};
    };
    if (kind == 0) {
      g.vertices.push_back(vertex(0, 2, 1)); g.vertices.push_back(vertex(1, 0, 2)); g.vertices.push_back(vertex(2, 1, 0));
    } else if (kind == 1) {
      auto a = vertex(0, 3, 1), b = vertex(1, 0, 2), c = vertex(2, 1, 3), d = vertex(3, 2, 0);
      g.vertices.insert(g.vertices.end(), {a, b, c, a, c, d});
    } else {
      for (std::size_t i = 1; i + 1 < count; ++i) {
        g.vertices.push_back(vertex(0, i + 1, i)); g.vertices.push_back(vertex(i, 0, i + 1)); g.vertices.push_back(vertex(i + 1, i, 0));
      }
    }
    batch.count += outputCount;
    return {};
  }
  Result<void> append(List<RenderPrimitive> const& primitives) {
    for (auto const& primitive : primitives) {
      auto result = std::visit([&](auto const& shape) -> Result<void> {
        using T = std::remove_cvref_t<decltype(shape)>;
        if constexpr (std::same_as<T, RenderPoly>) {
          return append(2, shape.vertexes.size(), shape.texture, [&](std::size_t i) -> RenderVertex const& { return shape.vertexes[i]; });
        } else {
          constexpr bool quad = std::same_as<T, RenderQuad>;
          return append(quad ? 1 : 0, quad ? 4 : 3, shape.texture, [&](std::size_t i) -> RenderVertex const& {
            if (!i) return shape.a;
            if (i == 1) return shape.b;
            if constexpr (quad) if (i == 3) return shape.d;
            return shape.c;
          });
        }
      }, primitive);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    return {};
  }
  Result<void> append(DetachedRenderGeometry const& input) {
    for (auto const& primitive : input.primitives) {
      if (primitive.kind > 2 || (primitive.kind == 0 && primitive.count != 3) || (primitive.kind == 1 && primitive.count != 4))
        return std::unexpected(RendererException("Invalid detached Vulkan primitive"));
      if (primitive.kind == 2 && primitive.count < 3) continue;
      if (primitive.first > input.vertices.size() || primitive.count > input.vertices.size() - primitive.first)
        return std::unexpected(RendererException("Detached Vulkan vertex range is invalid"));
      auto texture = std::cref(white);
      if (primitive.texture) {
        auto found = input.textures.find(primitive.texture);
        if (found == input.textures.end() || !found->second) return std::unexpected(RendererException("Unknown detached Vulkan texture"));
        texture = std::cref(found->second);
      }
      RENDER_REQUIRE(append(primitive.kind, primitive.count, texture.get(), [&](std::size_t i) -> RenderVertex const& { return input.vertices[primitive.first + i]; }));
    }
    return {};
  }
  Result<void> set(List<RenderPrimitive>& input) override { return replace(input); }
};
struct Parameter {
  std::uint8_t type = 0;
  std::uint32_t offset = 0;
  String uniform;
  std::optional<RenderEffectParameter> value;
  bool scriptable = false;
};
struct EffectTexture {
  std::uint32_t binding = 0, sizeOffset = 0;
  String uniform, sizeUniform;
  TextureAddressing addressing = TextureAddressing::Clamp;
  TextureFiltering filtering = TextureFiltering::Nearest;
  std::shared_ptr<GpuImage> image;
  std::vector<std::shared_ptr<GpuImage>> reusable;
};
struct PipelineKey {
  VkFormat format;
  VkSampleCountFlagBits samples;
  bool alpha;
  bool blend = true;
  auto operator<=>(PipelineKey const&) const = default;
};
struct Effect {
  std::shared_ptr<Device> device;
  Json config;
  VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptors = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  std::map<PipelineKey, VkPipeline> pipelines;
  std::map<String, Parameter> parameters;
  std::map<String, EffectTexture> textures;
  std::optional<String> output, blit;
  std::vector<std::pair<String, String>> inputs;
  std::uint32_t uniformSize = 128;
  bool includeTextures = true, pingPong = false;
  std::uint32_t outputOffset = 0;
  ~Effect() {
    for (auto const& [key, pipeline] : pipelines) vkDestroyPipeline(device->logical, pipeline, nullptr);
    if (layout) vkDestroyPipelineLayout(device->logical, layout, nullptr);
    if (descriptors) vkDestroyDescriptorSetLayout(device->logical, descriptors, nullptr);
    if (vertex) vkDestroyShaderModule(device->logical, vertex, nullptr);
    if (fragment) vkDestroyShaderModule(device->logical, fragment, nullptr);
  }
};
struct Target {
  std::shared_ptr<GpuImage> color;
  std::shared_ptr<GpuImage> resolve;
};
struct Framebuffer {
  Json config;
  std::optional<Vec2U> fixedSize;
  unsigned divisor = 1;
  bool clear = true, alpha = false, doubleBuffered = false;
  std::array<Target, 2> targets;
  unsigned current = 0;
};
struct RingChunk { std::shared_ptr<Buffer> buffer; VkDeviceSize used = 0; };
struct RingSlice { std::shared_ptr<Buffer> buffer; VkDeviceSize offset = 0; };
struct Frame {
  std::shared_ptr<Device> device;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer command = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkSemaphore acquired = VK_NULL_HANDLE;
  bool submitted = false;
  std::vector<VkDescriptorPool> descriptors;
  std::size_t descriptorPool = 0;
  std::vector<RingChunk> ring;
  std::vector<std::shared_ptr<GpuImage>> imagePins;
  std::vector<TexturePtr> texturePins;
  std::vector<std::shared_ptr<Geometry>> geometryPins;
  std::vector<std::shared_ptr<Effect>> effectPins;
  ~Frame() {
    if (submitted) vkWaitForFences(device->logical, 1, &fence, VK_TRUE, UINT64_MAX);
    for (auto descriptor : descriptors) vkDestroyDescriptorPool(device->logical, descriptor, nullptr);
    if (acquired) vkDestroySemaphore(device->logical, acquired, nullptr);
    if (fence) vkDestroyFence(device->logical, fence, nullptr);
    if (pool) vkDestroyCommandPool(device->logical, pool, nullptr);
  }
  Result<void> reset() {
    if (submitted) VK_REQUIRE(vkWaitForFences(device->logical, 1, &fence, VK_TRUE, UINT64_MAX));
    submitted = false;
    VK_REQUIRE(vkResetCommandPool(device->logical, pool, 0));
    for (auto descriptor : descriptors) VK_REQUIRE(vkResetDescriptorPool(device->logical, descriptor, 0));
    descriptorPool = 0;
    for (auto& chunk : ring) chunk.used = 0;
    imagePins.clear(); geometryPins.clear(); effectPins.clear(); texturePins.clear();
    return {};
  }
  Result<RingSlice> allocate(VkDeviceSize bytes, VkDeviceSize alignment) {
    for (auto& chunk : ring) {
      auto offset = (chunk.used + alignment - 1) & ~(alignment - 1);
      if (offset <= chunk.buffer->size && bytes <= chunk.buffer->size - offset) {
        chunk.used = offset + bytes;
        return RingSlice{chunk.buffer, offset};
      }
    }
    auto buffer = makeBuffer(device, std::max<VkDeviceSize>(4 * 1024 * 1024, bytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    if (!buffer) return std::unexpected(std::move(buffer.error()));
    ring.push_back({*buffer, bytes});
    return RingSlice{std::move(*buffer), 0};
  }
  Result<VkDescriptorSet> descriptor(VkDescriptorSetLayout layout, std::uint32_t samplerCount) {
    for (;;) {
      if (descriptorPool == descriptors.size()) {
        std::array<VkDescriptorPoolSize, 2> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::max(8192u, samplerCount)}}};
        VkDescriptorPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.maxSets = 512; info.poolSizeCount = sizes.size(); info.pPoolSizes = sizes.data();
        VkDescriptorPool pool;
        VK_REQUIRE(vkCreateDescriptorPool(device->logical, &info, nullptr, &pool));
        descriptors.push_back(pool);
      }
      VkDescriptorSetAllocateInfo info{};
      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      info.descriptorPool = descriptors[descriptorPool]; info.descriptorSetCount = 1; info.pSetLayouts = &layout;
      VkDescriptorSet set;
      auto result = vkAllocateDescriptorSets(device->logical, &info, &set);
      if (result == VK_SUCCESS) return set;
      if (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL) return vkError("descriptor allocation", result);
      ++descriptorPool;
    }
  }
};
constexpr std::string_view BasicVertex = R"GLSL(#version 450
layout(location=0) in vec2 vertexPosition;
layout(location=1) in vec2 vertexTextureCoordinate;
layout(location=2) in vec4 vertexColor;
layout(location=3) in int vertexData;
layout(location=0) out vec2 fragmentTextureCoordinate;
layout(location=1) flat out int fragmentTextureIndex;
layout(location=2) out vec4 fragmentColor;
void main() {
 vec2 p=(vertexTransform*vec3(vertexPosition,1)).xy;
 gl_Position=vec4(p/screenSize*2-1,0,1);
 int i=vertexData&3;
 vec2 s=i==3?textureSize3:i==2?textureSize2:i==1?textureSize1:textureSize0;
 fragmentTextureCoordinate=vertexTextureCoordinate/s;
 fragmentTextureIndex=i; fragmentColor=starVertexColor(vertexColor);
}
)GLSL";
constexpr std::string_view BasicFragment = R"GLSL(#version 450
layout(location=0) in vec2 fragmentTextureCoordinate;
layout(location=1) flat in int fragmentTextureIndex;
layout(location=2) in vec4 fragmentColor;
layout(location=0) out vec4 outColor;
void main() {
 vec4 c=fragmentTextureIndex==3?texture(texture3,fragmentTextureCoordinate):fragmentTextureIndex==2?texture(texture2,fragmentTextureCoordinate):fragmentTextureIndex==1?texture(texture1,fragmentTextureCoordinate):texture(texture0,fragmentTextureCoordinate);
 if(c.a<=0) discard;
 outColor=c*fragmentColor;
}
)GLSL";
constexpr std::string_view CopyFragment = R"GLSL(#version 450
layout(location=0) in vec2 fragmentTextureCoordinate;
layout(location=0) out vec4 outColor;
void main() { outColor=texture(texture0,fragmentTextureCoordinate); }
)GLSL";
// A single output transform, never a material/light-map operation. Scene
// luminance is unchanged through the knee and approaches the display ceiling
// monotonically. Scaling RGB together preserves neutral axes and scene hue;
// out-of-gamut highlights are desaturated toward their mapped luminance.
constexpr std::string_view OutputFragment = R"GLSL(#version 450
layout(location=0) in vec2 fragmentTextureCoordinate;
layout(location=0) out vec4 outColor;
void main() {
 vec4 c=texture(texture0,fragmentTextureCoordinate);
 vec3 rgb=max(c.rgb,vec3(0));
 if(starOutput.w==1.0) {
   rgb*=exp2(starOutput.x);
   float ceiling=starOutput.z;
   float knee=min(1.0,ceiling*0.75);
   float y=dot(rgb,vec3(0.2126,0.7152,0.0722));
   float mapped=y<=knee?y:knee+(ceiling-knee)*(y-knee)/(ceiling-knee+y-knee);
   rgb*=y>0.0?mapped/y:0.0;
   float largest=max(rgb.r,max(rgb.g,rgb.b));
   if(largest>ceiling) rgb=mix(vec3(mapped),rgb,(ceiling-mapped)/(largest-mapped));
   if(starOutput.y==1.0) rgb=starLinearToSrgb(rgb);
 } else if(starOutput.w==2.0) {
   // Linear BT.709 relative to paper white -> absolute BT.2020 -> ST2084.
   rgb=mat3(0.6274039,0.0690973,0.0163914,
            0.3292830,0.9195404,0.0880133,
            0.0433131,0.0113623,0.8955953)*rgb*starOutput.y;
   vec3 p=pow(clamp(rgb/10000.0,vec3(0),vec3(1)),vec3(2610.0/16384.0));
   rgb=pow((vec3(3424.0/4096.0)+(2413.0/128.0)*p)/(vec3(1)+(2392.0/128.0)*p),vec3(2523.0/32.0));
 }
 outColor=vec4(rgb,c.a);
}
)GLSL";
constexpr std::string_view GuiHdrVertex = R"GLSL(#version 450
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;
out gl_PerVertex { vec4 gl_Position; };
layout(location=0) out struct { vec4 Color; vec2 UV; } Out;
vec3 decode(vec3 c) { return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),greaterThan(c,vec3(0.04045))); }
void main() {
 Out.Color=vec4(decode(aColor.rgb),aColor.a); Out.UV=aUV;
 gl_Position=vec4(aPos*pc.uScale+pc.uTranslate,0,1);
}
)GLSL";
constexpr std::string_view GuiHdrFragment = R"GLSL(#version 450
layout(location=0) out vec4 fColor;
layout(set=0,binding=0) uniform texture2D _Texture;
layout(set=1,binding=0) uniform sampler _Sampler;
layout(location=0) in struct { vec4 Color; vec2 UV; } In;
vec3 decode(vec3 c) { return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),greaterThan(c,vec3(0.04045))); }
void main() {
 vec4 t=texture(sampler2D(_Texture,_Sampler),In.UV.st);
 fColor=vec4(In.Color.rgb*decode(t.rgb),In.Color.a*t.a);
}
)GLSL";
Result<VkShaderModule> compileShader(std::shared_ptr<Device> const&, String const&, String const&,
    std::string_view, shaderc_shader_kind, std::optional<std::reference_wrapper<std::vector<std::uint32_t>>> = {});
thread_local VkResult guiResult = VK_SUCCESS;
void checkGuiResult(VkResult result) {
  if (result < 0 && guiResult == VK_SUCCESS) guiResult = result;
}
} // namespace

struct VulkanRenderer::State {
  Logger& logger;
  SDL_Window& window;
  std::shared_ptr<Device> device = std::make_shared<Device>();
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFormat swapFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{};
  std::vector<VkImage> swapImages;
  std::vector<VkImageView> swapViews;
  std::vector<VkSemaphore> presentSemaphores;
  std::vector<VkImageLayout> swapLayouts;
  std::array<std::unique_ptr<Frame>, 2> frames;
  std::size_t frameIndex = 0;
  std::uint32_t imageIndex = 0;
  std::optional<std::reference_wrapper<Frame>> active;
  bool rendering = false, vsync = true, dirtySwapchain = true, gui = false, frameOpen = false;
  bool sizeLimit = false, multi = true, hdr = true;
  unsigned samples = 0;
  RenderOutputSettings outputSettings;
  VkFormat guiFormat = VK_FORMAT_UNDEFINED;
  std::vector<std::uint32_t> guiHdrShader;
  std::vector<std::uint32_t> guiHdrVertexShader;
  Target guiComposition, encodedOutput;
  Vec4F outputTransform{0, 0, 0, 0};
  bool transforming = false;
  Vec2U size{0, 0};
  Json config = JsonObject();
  std::map<String, std::shared_ptr<Effect>> effects;
  std::map<String, Framebuffer> framebuffers;
  String currentEffect = "internal";
  std::optional<String> output;
  Target screen;
  std::optional<Target> copyTarget;
  bool copying = false;
  TexturePtr white;
  VkRenderBuffer immediate;
  std::shared_ptr<Geometry> copyGeometry = std::make_shared<Geometry>();
  std::vector<std::shared_ptr<GpuImage>> sampledImages;
  std::vector<VkDescriptorImageInfo> imageInfos;
  std::vector<VkWriteDescriptorSet> descriptorWrites;
  List<RenderPrimitive> primitives;
  std::optional<RectI> scissor;
  State(Logger& log, SDL_Window& win, bool sync) : logger(log), window(win), vsync(sync) {}
  ~State();
  Result<void> initialize();
  Result<void> recreateSwapchain();
  void updateHdrMetadata();
  Result<std::map<String, Framebuffer>> createTargets(Json const&);
  void installTargets(Json const&, std::map<String, Framebuffer>);
  Result<void> rebuildTargets(Json const&);
  Result<Target> makeTarget(Vec2U, VkFormat, VkSampleCountFlagBits, TextureAddressing, TextureFiltering, bool alpha = true);
  Result<void> initializeGui(VkFormat = VK_FORMAT_UNDEFINED, bool hdrOutput = false);
  Result<void> loadEffect(String const&, Json const&, StringMap<String> const&);
  Result<void> upload(std::shared_ptr<GpuImage> const&);
  Result<void> replacePixels(std::shared_ptr<GpuImage> const&, ImageView const&);
  void barrier(GpuImage&, VkImageLayout);
  void endRendering();
  Result<void> beginRendering(Target const&, bool alpha);
  Result<void> clear(Target const&, bool alpha);
  Result<void> blit(Target const&, Target const&, bool flip = false);
  Result<void> transform(Target const&, Target const&, Vec4F const&, bool flip = false);
  Result<void> draw(std::shared_ptr<Geometry> const&, Mat3F const&, bool retained);
  Result<VkPipeline> pipeline(Effect&, PipelineKey);
  Result<std::reference_wrapper<Target>> target();
  Result<void> flush(Mat3F const& = Mat3F::identity());
  Result<void> chooseEffect(String const&);
};

VulkanRenderer::State::~State() {
  if (device->logical) vkDeviceWaitIdle(device->logical);
  if (gui) ImGui_ImplVulkan_Shutdown();
  active.reset();
  for (auto& frame : frames) frame.reset();
  effects.clear(); framebuffers.clear(); screen = {};
  immediate.geometry.reset(); immediate.white.reset(); white.reset();
  for (auto view : swapViews) vkDestroyImageView(device->logical, view, nullptr);
  for (auto semaphore : presentSemaphores) vkDestroySemaphore(device->logical, semaphore, nullptr);
  if (swapchain) vkDestroySwapchainKHR(device->logical, swapchain, nullptr);
  if (device->surface) { vkDestroySurfaceKHR(device->instance, device->surface, nullptr); device->surface = VK_NULL_HANDLE; }
}

Result<void> VulkanRenderer::State::initialize() {
  std::uint32_t extensionCount = 0;
  auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  if (!extensions) return std::unexpected(RendererException::format("SDL Vulkan extensions: {}", SDL_GetError()));
  std::vector<char const*> instanceExtensions(extensions, extensions + extensionCount);
  std::uint32_t availableCount = 0;
  VK_REQUIRE(vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr));
  std::vector<VkExtensionProperties> availableExtensions(availableCount);
  VK_REQUIRE(vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, availableExtensions.data()));
  bool enumeratePortability = std::ranges::any_of(availableExtensions, [](auto const& extension) {
    return std::string_view(extension.extensionName) == VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
  });
  if (enumeratePortability) instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  device->swapchainColorspace = std::ranges::any_of(availableExtensions, [](auto const& extension) {
    return std::string_view(extension.extensionName) == VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
  });
  if (device->swapchainColorspace) instanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
  VkApplicationInfo application{};
  application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application.pApplicationName = "Asterbound"; application.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo instance{};
  instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance.pApplicationInfo = &application; instance.enabledExtensionCount = instanceExtensions.size(); instance.ppEnabledExtensionNames = instanceExtensions.data();
  if (enumeratePortability) instance.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  VK_REQUIRE(vkCreateInstance(&instance, nullptr, &device->instance));
  if (!SDL_Vulkan_CreateSurface(&window, device->instance, nullptr, &device->surface))
    return std::unexpected(RendererException::format("SDL Vulkan surface: {}", SDL_GetError()));
  std::uint32_t count = 0;
  VK_REQUIRE(vkEnumeratePhysicalDevices(device->instance, &count, nullptr));
  std::vector<VkPhysicalDevice> physicalDevices(count);
  VK_REQUIRE(vkEnumeratePhysicalDevices(device->instance, &count, physicalDevices.data()));
  int bestScore = -1;
  for (auto physical : physicalDevices) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_3) continue;
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features13;
    vkGetPhysicalDeviceFeatures2(physical, &features);
    if (!features13.dynamicRendering) continue;
    std::uint32_t extensionCount = 0;
    VK_REQUIRE(vkEnumerateDeviceExtensionProperties(physical, nullptr, &extensionCount, nullptr));
    std::vector<VkExtensionProperties> available(extensionCount);
    VK_REQUIRE(vkEnumerateDeviceExtensionProperties(physical, nullptr, &extensionCount, available.data()));
    auto has = [&](std::string_view name) {
      return std::ranges::any_of(available, [&](auto const& extension) { return name == extension.extensionName; });
    };
    if (!has(VK_KHR_SWAPCHAIN_EXTENSION_NAME) || !has(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) continue;
    std::uint32_t families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, nullptr);
    std::vector<VkQueueFamilyProperties> queues(families);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, queues.data());
    for (std::uint32_t family = 0; family < families; ++family) {
      VkBool32 present = false;
      VK_REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(physical, family, device->surface, &present));
      if (!(queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present) continue;
      int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
          : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
      if (score > bestScore) {
        bestScore = score; device->physical = physical; device->family = family;
        device->properties = properties; device->sampleShading = features.features.sampleRateShading;
        device->portability = has("VK_KHR_portability_subset");
        device->hdrMetadata = has(VK_EXT_HDR_METADATA_EXTENSION_NAME);
      }
    }
  }
  if (!device->physical)
    return std::unexpected(RendererException("Vulkan requires a Vulkan 1.3 device with dynamic rendering and a graphics/present queue"));
  float priority = 1;
  VkDeviceQueueCreateInfo queue{};
  queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue.queueFamilyIndex = device->family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
  VkPhysicalDeviceVulkan13Features enabled13{};
  enabled13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  enabled13.dynamicRendering = VK_TRUE;
  VkPhysicalDeviceFeatures enabled{};
  enabled.sampleRateShading = device->sampleShading;
  std::vector<char const*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};
  if (device->portability) deviceExtensions.push_back("VK_KHR_portability_subset");
  if (device->hdrMetadata) deviceExtensions.push_back(VK_EXT_HDR_METADATA_EXTENSION_NAME);
  VkDeviceCreateInfo logical{};
  logical.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  logical.pNext = &enabled13; logical.queueCreateInfoCount = 1; logical.pQueueCreateInfos = &queue;
  logical.enabledExtensionCount = deviceExtensions.size(); logical.ppEnabledExtensionNames = deviceExtensions.data(); logical.pEnabledFeatures = &enabled;
  VK_REQUIRE(vkCreateDevice(device->physical, &logical, nullptr, &device->logical));
  vkGetDeviceQueue(device->logical, device->family, 0, &device->queue);
  vkGetPhysicalDeviceMemoryProperties(device->physical, &device->memory);
  RENDER_REQUIRE(logger.info("Vulkan device: '{}' API {}.{}.{}", device->properties.deviceName,
      VK_API_VERSION_MAJOR(device->properties.apiVersion), VK_API_VERSION_MINOR(device->properties.apiVersion), VK_API_VERSION_PATCH(device->properties.apiVersion)));
  for (auto& slot : frames) {
    slot = std::make_unique<Frame>(); slot->device = device;
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pool.queueFamilyIndex = device->family;
    VK_REQUIRE(vkCreateCommandPool(device->logical, &pool, nullptr, &slot->pool));
    VkCommandBufferAllocateInfo command{};
    command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command.commandPool = slot->pool; command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; command.commandBufferCount = 1;
    VK_REQUIRE(vkAllocateCommandBuffers(device->logical, &command, &slot->command));
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_REQUIRE(vkCreateFence(device->logical, &fence, nullptr, &slot->fence));
    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_REQUIRE(vkCreateSemaphore(device->logical, &semaphore, nullptr, &slot->acquired));
  }
  auto texture = make_ref<VkTexture>();
  texture->dimensions = Vec2U(1, 1);
  auto image = imageFromPixels(device, ImageView(Image::filled({1, 1}, Vec4B(255, 255, 255, 255), PixelFormat::RGBA32)), TextureAddressing::Clamp, TextureFiltering::Nearest);
  if (!image) return std::unexpected(std::move(image.error()));
  texture->image = std::move(*image); white = TexturePtr(std::move(texture));
  immediate.device = device; immediate.white = white;
  RENDER_REQUIRE(loadEffect("internal", JsonObject(), {{"vertex", String(BasicVertex)}, {"fragment", String(BasicFragment)}}));
  RENDER_REQUIRE(loadEffect("internalCopy", JsonObject(), {{"vertex", String(BasicVertex)}, {"fragment", String(CopyFragment)}}));
  RENDER_REQUIRE(loadEffect("internalOutput", JsonObject(), {{"vertex", String(BasicVertex)}, {"fragment", String(OutputFragment)}}));
  currentEffect = "internal";
  int width = 0, height = 0;
  if (!SDL_GetWindowSizeInPixels(&window, &width, &height))
    return std::unexpected(RendererException::format("SDL Vulkan drawable size: {}", SDL_GetError()));
  size = Vec2U(std::max(width, 0), std::max(height, 0));
  return recreateSwapchain();
}

Result<Target> VulkanRenderer::State::makeTarget(Vec2U dimensions, VkFormat format, VkSampleCountFlagBits sampleCount,
    TextureAddressing addressing, TextureFiltering filtering, bool alpha) {
  if (dimensions[0] > device->properties.limits.maxViewportDimensions[0] || dimensions[1] > device->properties.limits.maxViewportDimensions[1])
    return std::unexpected(RendererException("Vulkan framebuffer dimensions exceed device viewport limits"));
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(device->physical, format, &properties);
  if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT))
    return std::unexpected(RendererException::format("Vulkan framebuffer format {} does not support alpha blending", static_cast<int>(format)));
  auto color = makeImage(device, dimensions, format, sampleCount, addressing, filtering);
  if (!color) return std::unexpected(std::move(color.error()));
  Target target{*color, *color};
  if (sampleCount != VK_SAMPLE_COUNT_1_BIT) {
    auto resolve = makeImage(device, dimensions, format, VK_SAMPLE_COUNT_1_BIT, addressing, filtering);
    if (!resolve) return std::unexpected(std::move(resolve.error()));
    target.resolve = std::move(*resolve);
  }
  if (!alpha) {
    auto& image = *target.resolve;
    image.opaque = true;
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = image.handle; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = image.format;
    view.components.a = VK_COMPONENT_SWIZZLE_ONE;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_REQUIRE(vkCreateImageView(device->logical, &view, nullptr, &image.sampledView));
  }
  return target;
}

Result<void> VulkanRenderer::State::recreateSwapchain() {
  if (active) return std::unexpected(RendererException("Cannot recreate Vulkan swapchain inside a frame"));
  if (!size[0] || !size[1]) return {};
  VK_REQUIRE(vkDeviceWaitIdle(device->logical));
  VkSurfaceCapabilitiesKHR capabilities;
  VK_REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical, device->surface, &capabilities));
  VkExtent2D newExtent = capabilities.currentExtent;
  if (newExtent.width == UINT32_MAX) {
    newExtent.width = std::clamp(size[0], capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    newExtent.height = std::clamp(size[1], capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  }
  if (!newExtent.width || !newExtent.height)
    return std::unexpected(RendererException("Vulkan surface has no drawable extent; restore the window before changing output settings"));
  if ((capabilities.supportedUsageFlags & (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) != (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
    return std::unexpected(RendererException("Vulkan surface does not support color attachment presentation image transfers"));
  std::uint32_t count = 0;
  VK_REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical, device->surface, &count, nullptr));
  std::vector<VkSurfaceFormatKHR> formats(count);
  VK_REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical, device->surface, &count, formats.data()));
  if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) formats[0].format = VK_FORMAT_B8G8R8A8_UNORM;
  bool hdrOutput = outputSettings.mode == RenderOutputMode::Hdr10;
  if (hdrOutput && !device->swapchainColorspace)
    return std::unexpected(RendererException("HDR10 requires VK_EXT_swapchain_colorspace; use SDR or a Vulkan driver/window system exposing HDR output"));
  auto chosen = std::find_if(formats.begin(), formats.end(), [hdrOutput](auto const& format) {
    if (hdrOutput)
      return (format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32)
          && format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
    return (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  });
  if (chosen == formats.end())
    return std::unexpected(RendererException(hdrOutput
        ? "HDR10 unavailable: this Vulkan surface does not advertise HDR10 ST2084 with a 10-bit UNORM format; enable HDR in a capable display/compositor or select SDR"
        : "Vulkan surface has no non-sRGB RGBA8/BGRA8 presentation format"));
  VkFormatProperties formatSupport;
  vkGetPhysicalDeviceFormatProperties(device->physical, chosen->format, &formatSupport);
  if (!(formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
    return std::unexpected(RendererException("Vulkan presentation format cannot receive a color blit"));
  VK_REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical, device->surface, &count, nullptr));
  std::vector<VkPresentModeKHR> modes(count);
  VK_REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical, device->surface, &count, modes.data()));
  VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
  if (!vsync) {
    if (std::ranges::find(modes, VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    else if (std::ranges::find(modes, VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) mode = VK_PRESENT_MODE_MAILBOX_KHR;
    else return std::unexpected(RendererException("Vulkan surface cannot disable VSync: no immediate or mailbox presentation mode"));
  }
  auto imageCount = std::max(2u, capabilities.minImageCount + 1);
  if (capabilities.maxImageCount) imageCount = std::min(imageCount, capabilities.maxImageCount);
  if (imageCount < 2) return std::unexpected(RendererException("Vulkan ImGui requires at least two swapchain images"));
  VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  for (auto candidate : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
    if (capabilities.supportedCompositeAlpha & candidate) { composite = candidate; break; }
  auto dimensions = Vec2U(newExtent.width, newExtent.height);
  auto screenTarget = makeTarget(dimensions, hdrOutput ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM,
      VK_SAMPLE_COUNT_1_BIT, TextureAddressing::Clamp, TextureFiltering::Nearest);
  if (!screenTarget) return std::unexpected(std::move(screenTarget.error()));
  Target compositionTarget, encodedTarget;
  if (hdrOutput) {
    auto composition = makeTarget(dimensions, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, TextureAddressing::Clamp, TextureFiltering::Nearest);
    if (!composition) return std::unexpected(std::move(composition.error()));
    compositionTarget = std::move(*composition);
    auto encoded = makeTarget(dimensions, chosen->format, VK_SAMPLE_COUNT_1_BIT, TextureAddressing::Clamp, TextureFiltering::Nearest);
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    encodedTarget = std::move(*encoded);
  }
  auto targets = createTargets(config);
  if (!targets) return std::unexpected(std::move(targets.error()));
  if (outputSettings.mode != RenderOutputMode::Legacy) {
    auto mapped = pipeline(*effects.at("internalOutput"), {screenTarget->color->format, VK_SAMPLE_COUNT_1_BIT, true, false});
    if (!mapped) return std::unexpected(std::move(mapped.error()));
    if (hdrOutput) {
      auto encoded = pipeline(*effects.at("internalOutput"), {chosen->format, VK_SAMPLE_COUNT_1_BIT, true, false});
      if (!encoded) return std::unexpected(std::move(encoded.error()));
    }
  }
  VkSwapchainCreateInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.surface = device->surface; info.minImageCount = imageCount; info.imageFormat = chosen->format; info.imageColorSpace = chosen->colorSpace;
  info.imageExtent = newExtent; info.imageArrayLayers = 1; info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; info.preTransform = capabilities.currentTransform;
  info.compositeAlpha = composite; info.presentMode = mode; info.clipped = VK_TRUE;
  info.oldSwapchain = swapchain;
  // Passing oldSwapchain retires it even if creation fails. Prepare independent
  // resources first, then remove the old generation from acquire/present state.
  struct SwapchainGuard {
    VkDevice device; VkSwapchainKHR swapchain;
    std::vector<VkImageView> views;
    std::vector<VkSemaphore> semaphores;
    ~SwapchainGuard() {
      for (auto view : views) vkDestroyImageView(device, view, nullptr);
      for (auto semaphore : semaphores) vkDestroySemaphore(device, semaphore, nullptr);
      if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    }
  };
  SwapchainGuard retired{device->logical, std::exchange(swapchain, VK_NULL_HANDLE),
      std::move(swapViews), std::move(presentSemaphores)};
  swapImages.clear(); swapLayouts.clear();
  dirtySwapchain = true;
  SwapchainGuard guard{device->logical, VK_NULL_HANDLE, {}, {}};
  VK_REQUIRE(vkCreateSwapchainKHR(device->logical, &info, nullptr, &guard.swapchain));
  VK_REQUIRE(vkGetSwapchainImagesKHR(device->logical, guard.swapchain, &count, nullptr));
  std::vector<VkImage> images(count);
  VK_REQUIRE(vkGetSwapchainImagesKHR(device->logical, guard.swapchain, &count, images.data()));
  for (auto image : images) {
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = chosen->format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView handle;
    VK_REQUIRE(vkCreateImageView(device->logical, &view, nullptr, &handle)); guard.views.push_back(handle);
    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore ready;
    VK_REQUIRE(vkCreateSemaphore(device->logical, &semaphore, nullptr, &ready)); guard.semaphores.push_back(ready);
  }
  auto newGuiFormat = hdrOutput ? VK_FORMAT_R16G16B16A16_SFLOAT : chosen->format;
  if (gui && newGuiFormat != guiFormat) {
    auto oldFormat = guiFormat;
    ImGui_ImplVulkan_Shutdown(); gui = false;
    auto initialized = initializeGui(newGuiFormat, hdrOutput);
    if (!initialized) {
      if (gui) { ImGui_ImplVulkan_Shutdown(); gui = false; }
      auto restored = initializeGui(oldFormat, oldFormat == VK_FORMAT_R16G16B16A16_SFLOAT);
      if (!restored)
        return std::unexpected(RendererException::format("Vulkan output change failed: {}; restoring ImGui also failed: {}", initialized.error().what(), restored.error().what()));
      return std::unexpected(std::move(initialized.error()));
    }
  }
  swapchain = std::exchange(guard.swapchain, VK_NULL_HANDLE);
  swapViews = std::move(guard.views); presentSemaphores = std::move(guard.semaphores);
  swapImages = std::move(images); swapLayouts.assign(swapImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
  swapFormat = chosen->format; extent = newExtent; screen = std::move(*screenTarget);
  guiComposition = std::move(compositionTarget); encodedOutput = std::move(encodedTarget);
  installTargets(config, std::move(*targets));
  dirtySwapchain = false;
  updateHdrMetadata();
  return {};
}

void VulkanRenderer::State::updateHdrMetadata() {
  if (outputSettings.mode == RenderOutputMode::Hdr10 && device->hdrMetadata) {
    auto setMetadata = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(vkGetDeviceProcAddr(device->logical, "vkSetHdrMetadataEXT"));
    if (setMetadata) {
      VkHdrMetadataEXT metadata{};
      metadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
      metadata.displayPrimaryRed = {0.708f, 0.292f}; metadata.displayPrimaryGreen = {0.170f, 0.797f};
      metadata.displayPrimaryBlue = {0.131f, 0.046f}; metadata.whitePoint = {0.3127f, 0.3290f};
      metadata.maxLuminance = outputSettings.peakNits; metadata.minLuminance = 0;
      metadata.maxContentLightLevel = outputSettings.peakNits;
      // No measured MaxFALL is available. Zero leaves it unspecified rather
      // than claiming that paper-white or peak is a measured frame average.
      metadata.maxFrameAverageLightLevel = 0;
      setMetadata(device->logical, 1, &swapchain, &metadata);
    }
  }
}

Result<void> VulkanRenderer::State::initializeGui(VkFormat format, bool hdrOutput) {
  if (gui) return {};
  if (format == VK_FORMAT_UNDEFINED) {
    hdrOutput = outputSettings.mode == RenderOutputMode::Hdr10;
    format = hdrOutput ? VK_FORMAT_R16G16B16A16_SFLOAT : swapFormat;
  }
  if (format == VK_FORMAT_UNDEFINED) format = VK_FORMAT_B8G8R8A8_UNORM;
  if (hdrOutput && guiHdrShader.empty()) {
    auto shader = compileShader(device, "internalImGuiHdr", String(GuiHdrFragment), {}, shaderc_fragment_shader, std::ref(guiHdrShader));
    if (!shader) return std::unexpected(std::move(shader.error()));
    vkDestroyShaderModule(device->logical, *shader, nullptr);
    auto vertex = compileShader(device, "internalImGuiHdrVertex", String(GuiHdrVertex), {}, shaderc_vertex_shader, std::ref(guiHdrVertexShader));
    if (!vertex) { guiHdrShader.clear(); return std::unexpected(std::move(vertex.error())); }
    vkDestroyShaderModule(device->logical, *vertex, nullptr);
  }
  guiFormat = format;
  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion = VK_API_VERSION_1_3; info.Instance = device->instance; info.PhysicalDevice = device->physical;
  info.Device = device->logical; info.QueueFamily = device->family; info.Queue = device->queue;
  info.DescriptorPoolSize = 1024; info.MinImageCount = 2; info.ImageCount = std::max<std::size_t>(2, swapImages.size());
  info.UseDynamicRendering = true;
  info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  auto& renderingInfo = info.PipelineInfoMain.PipelineRenderingCreateInfo;
  renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingInfo.colorAttachmentCount = 1; renderingInfo.pColorAttachmentFormats = &guiFormat;
  if (hdrOutput) {
    info.CustomShaderFragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.CustomShaderFragCreateInfo.codeSize = guiHdrShader.size() * sizeof(std::uint32_t);
    info.CustomShaderFragCreateInfo.pCode = guiHdrShader.data();
    info.CustomShaderVertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.CustomShaderVertCreateInfo.codeSize = guiHdrVertexShader.size() * sizeof(std::uint32_t);
    info.CustomShaderVertCreateInfo.pCode = guiHdrVertexShader.data();
  }
  info.MinAllocationSize = 1024 * 1024;
  guiResult = VK_SUCCESS; info.CheckVkResultFn = checkGuiResult;
  if (!ImGui_ImplVulkan_Init(&info))
    return std::unexpected(RendererException("Could not initialize ImGui Vulkan renderer"));
  gui = true;
  if (guiResult != VK_SUCCESS) return vkError("ImGui initialization", guiResult);
  return {};
}

namespace {
bool shaderIdentifier(String const& name) {
  std::string_view text = name.utf8();
  if (text.empty() || !(text[0] == '_' || (text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z'))) return false;
  return std::ranges::all_of(text, [](char c) { return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); });
}
Result<RenderEffectParameter> parseParameter(String const& type, Json const& value) {
  if (type == "bool") {
    auto parsed = value.to_bool(); if (!parsed) return std::unexpected(std::move(parsed.error()));
    return RenderEffectParameter(*parsed);
  } else if (type == "int") {
    auto parsed = value.to_int(); if (!parsed) return std::unexpected(std::move(parsed.error()));
    if (*parsed < std::numeric_limits<int>::min() || *parsed > std::numeric_limits<int>::max())
      return std::unexpected(RendererException("Vulkan integer effect parameter is outside int32 range"));
    return RenderEffectParameter(static_cast<int>(*parsed));
  } else if (type == "float") {
    auto parsed = value.to_float(); if (!parsed) return std::unexpected(std::move(parsed.error()));
    return RenderEffectParameter(static_cast<float>(*parsed));
  } else if (type == "vec2") {
    auto parsed = json_to_vec2f(value); if (!parsed) return std::unexpected(std::move(parsed.error()));
    return RenderEffectParameter(*parsed);
  } else if (type == "vec3") {
    auto parsed = json_to_vec3f(value); if (!parsed) return std::unexpected(std::move(parsed.error()));
    return RenderEffectParameter(*parsed);
  } else if (type == "vec4") {
    auto parsed = json_to_vec4f(value); if (!parsed) return std::unexpected(std::move(parsed.error()));
    return RenderEffectParameter(*parsed);
  }
  return std::unexpected(RendererException::format("Unknown Vulkan effect parameter type '{}'", type));
}
Result<VkShaderModule> compileShader(std::shared_ptr<Device> const& device, String const& name, String const& source,
    std::string_view declarations, shaderc_shader_kind kind, std::optional<std::reference_wrapper<std::vector<std::uint32_t>>> retainedWords) {
  std::string_view text = source.utf8();
  auto lineEnd = text.find('\n');
  if (!text.starts_with("#version 450") || lineEnd == std::string_view::npos)
    return std::unexpected(RendererException::format("Vulkan shader '{}' must be an explicit #version 450 variant; legacy OpenGL GLSL is not supported", name));
  auto insertion = lineEnd + 1;
  unsigned originalLine = 2;
  bool inComment = false;
  while (insertion < text.size()) {
    auto end = text.find('\n', insertion);
    if (end == std::string_view::npos) break;
    auto line = text.substr(insertion, end - insertion);
    for (;;) {
      if (inComment) {
        auto close = line.find("*/");
        if (close == std::string_view::npos) { line = {}; break; }
        line.remove_prefix(close + 2); inComment = false;
      }
      auto first = line.find_first_not_of(" \t\r");
      line = first == std::string_view::npos ? std::string_view{} : line.substr(first);
      if (!line.starts_with("/*")) break;
      line.remove_prefix(2); inComment = true;
    }
    if (!line.empty() && !line.starts_with("//") && !line.starts_with("#extension") && !line.starts_with("#define") && !line.starts_with("#pragma")) {
      // Include a closing leading block-comment delimiter before declarations,
      // even when the first GLSL token follows it on the same source line.
      insertion = static_cast<std::size_t>(line.data() - text.data());
      break;
    }
    insertion = end + 1; ++originalLine;
  }
  std::string expanded;
  expanded.reserve(text.size() + declarations.size() + 32);
  expanded.append(text.substr(0, insertion)); expanded.append(declarations);
  expanded += std::format("\n#line {}\n", originalLine); expanded.append(text.substr(insertion));
  struct Compiler {
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    shaderc_compile_options_t options = shaderc_compile_options_initialize();
    shaderc_compilation_result_t result = nullptr;
    ~Compiler() {
      if (result) shaderc_result_release(result);
      if (options) shaderc_compile_options_release(options);
      if (compiler) shaderc_compiler_release(compiler);
    }
  } compiler;
  if (!compiler.compiler || !compiler.options) return std::unexpected(RendererException("Could not allocate shaderc compiler"));
  // Dynamic rendering requires the 1.3 API, not optional 1.3 shader features.
  // Keep discard as OpKill instead of requiring shaderDemoteToHelperInvocation.
  shaderc_compile_options_set_target_env(compiler.options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
  shaderc_compile_options_set_target_spirv(compiler.options, shaderc_spirv_version_1_5);
  shaderc_compile_options_set_source_language(compiler.options, shaderc_source_language_glsl);
  compiler.result = shaderc_compile_into_spv(compiler.compiler, expanded.data(), expanded.size(), kind, name.utf8().c_str(), "main", compiler.options);
  if (!compiler.result) return std::unexpected(RendererException::format("shaderc returned no result for '{}'", name));
  if (shaderc_result_get_compilation_status(compiler.result) != shaderc_compilation_status_success)
    return std::unexpected(RendererException::format("Vulkan shader '{}': {}", name, shaderc_result_get_error_message(compiler.result)));
  auto bytes = shaderc_result_get_length(compiler.result);
  if (bytes % sizeof(std::uint32_t)) return std::unexpected(RendererException("shaderc returned unaligned SPIR-V"));
  std::vector<std::uint32_t> words(bytes / sizeof(std::uint32_t));
  std::memcpy(words.data(), shaderc_result_get_bytes(compiler.result), bytes);
  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = bytes; info.pCode = words.data();
  VkShaderModule module;
  VK_REQUIRE(vkCreateShaderModule(device->logical, &info, nullptr, &module));
  if (retainedWords) retainedWords->get() = std::move(words);
  return module;
}
}

#define JSON_REQUIRE(name, expression) auto name = (expression); if (!name) return std::unexpected(std::move(name.error()))
Result<void> VulkanRenderer::State::loadEffect(String const& name, Json const& definition, StringMap<String> const& shaders) {
  auto vertexSource = shaders.find_ref("vertex"), fragmentSource = shaders.find_ref("fragment");
  if (!vertexSource || !fragmentSource)
    return std::unexpected(RendererException::format("Vulkan effect '{}' requires explicit vertex and fragment shaders in effectShadersVulkan", name));
  auto effect = std::make_shared<Effect>();
  effect->device = device; effect->config = definition;
  JSON_REQUIRE(include, definition.get_bool("includeVBTextures", true));
  effect->includeTextures = *include;
  JSON_REQUIRE(outputName, definition.opt_string("frameBuffer")); effect->output = *outputName;
  JSON_REQUIRE(blitName, definition.opt_string("blitFrameBuffer")); effect->blit = *blitName;
  effect->pingPong = effect->output && effect->blit && *effect->output == *effect->blit;
  JSON_REQUIRE(inputs, definition.get_array("frameBufferTextures", {}));
  for (auto const& input : *inputs) {
    JSON_REQUIRE(framebuffer, input.opt_string("framebuffer"));
    if (!*framebuffer) continue;
    JSON_REQUIRE(texture, input.get_string("texture"));
    effect->inputs.emplace_back(*texture, **framebuffer);
    if (effect->output && **framebuffer == *effect->output) effect->pingPong = true;
  }
  std::string declarations = "layout(std140,set=0,binding=0) uniform StarUniforms {\n"
    "layout(offset=0) vec2 screenSize;\nlayout(offset=16) mat3 vertexTransform;\n"
    "layout(offset=64) vec2 textureSize0;\nlayout(offset=80) vec2 textureSize1;\n"
    "layout(offset=96) vec2 textureSize2;\nlayout(offset=112) vec2 textureSize3;\n";
  std::set<String> uniformNames{"screenSize", "vertexTransform", "textureSize0", "textureSize1", "textureSize2", "textureSize3", "texture0", "texture1", "texture2", "texture3", "starColorState", "starOutput"};
  JSON_REQUIRE(parameters, definition.get_object("effectParameters", {}));
  // Sort public names so the descriptor/uniform ABI does not depend on hash-map
  // insertion order or JSON object serialization order.
  for (auto const& [parameterName, value] : *parameters) effect->parameters.emplace(parameterName, Parameter{});
  for (auto& [parameterName, parameter] : effect->parameters) {
    auto found = parameters->find_ref(parameterName);
    auto const& value = found->get();
    JSON_REQUIRE(uniform, value.get_string("uniform"));
    JSON_REQUIRE(type, value.get_string("type"));
    JSON_REQUIRE(scriptable, value.get_bool("scriptable", false));
    if (!shaderIdentifier(*uniform) || !uniformNames.insert(*uniform).second)
      return std::unexpected(RendererException::format("Invalid or duplicate Vulkan uniform '{}'", *uniform));
    parameter.uniform = *uniform; parameter.scriptable = *scriptable; parameter.offset = effect->uniformSize;
    if (*type == "bool") parameter.type = 5;
    else if (*type == "int") parameter.type = 1;
    else if (*type == "float") parameter.type = 0;
    else if (*type == "vec2") parameter.type = 4;
    else if (*type == "vec3") parameter.type = 3;
    else if (*type == "vec4") parameter.type = 2;
    else return std::unexpected(RendererException::format("Unrecognized Vulkan effect parameter type '{}'", *type));
    declarations += std::format("layout(offset={}) {} {};\n", effect->uniformSize, type->utf8(), uniform->utf8());
    effect->uniformSize += 16;
    JSON_REQUIRE(defaultValue, value.get("default", {}));
    if (!defaultValue->is_null()) {
      auto parsed = parseParameter(*type, *defaultValue);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      parameter.value = std::move(*parsed);
    }
  }
  JSON_REQUIRE(textures, definition.get_object("effectTextures", {}));
  for (auto const& [textureName, value] : *textures) effect->textures.emplace(textureName, EffectTexture{});
  std::uint32_t binding = 5;
  for (auto& [textureName, texture] : effect->textures) {
    auto const& value = textures->find_ref(textureName)->get();
    JSON_REQUIRE(uniform, value.get_string("textureUniform"));
    JSON_REQUIRE(sizeUniform, value.opt_string("textureSizeUniform"));
    JSON_REQUIRE(addressingName, value.get_string("textureAddressing", "clamp"));
    JSON_REQUIRE(filteringName, value.get_string("textureFiltering", "nearest"));
    JSON_REQUIRE(addressing, enum_value<TextureAddressingNames>(*addressingName));
    JSON_REQUIRE(filtering, enum_value<TextureFilteringNames>(*filteringName));
    if (!shaderIdentifier(*uniform) || !uniformNames.insert(*uniform).second)
      return std::unexpected(RendererException::format("Invalid or duplicate Vulkan texture uniform '{}'", *uniform));
    texture.uniform = *uniform; texture.binding = binding++;
    texture.addressing = *addressing; texture.filtering = *filtering;
    if (*sizeUniform) {
      if (!shaderIdentifier(**sizeUniform) || !uniformNames.insert(**sizeUniform).second)
        return std::unexpected(RendererException::format("Invalid or duplicate Vulkan texture size uniform '{}'", **sizeUniform));
      texture.sizeUniform = **sizeUniform; texture.sizeOffset = effect->uniformSize;
      declarations += std::format("layout(offset={}) vec2 {};\n", effect->uniformSize, texture.sizeUniform.utf8());
      effect->uniformSize += 16;
    }
  }
  effect->outputOffset = effect->uniformSize;
  declarations += std::format("layout(offset={}) vec4 starColorState;\nlayout(offset={}) vec4 starOutput;\n}};\n",
      effect->uniformSize, effect->uniformSize + 16);
  effect->uniformSize += 32;
  declarations += R"GLSL(
vec3 starSrgbToLinear(vec3 c) { return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),greaterThan(c,vec3(0.04045))); }
vec3 starLinearToSrgb(vec3 c) { return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-0.055,greaterThan(c,vec3(0.0031308))); }
vec4 starVertexColor(vec4 c) { return vec4(starColorState.x!=0.0?starSrgbToLinear(c.rgb):c.rgb,c.a); }
)GLSL";
  for (unsigned i = 0; i < 4; ++i)
    declarations += std::format("layout(set=0,binding={}) uniform sampler2D texture{};\n", i + 1, i);
  for (auto const& [textureName, texture] : effect->textures)
    declarations += std::format("layout(set=0,binding={}) uniform sampler2D {};\n", texture.binding, texture.uniform.utf8());
  if (effect->uniformSize > device->properties.limits.maxUniformBufferRange || binding - 1 > device->properties.limits.maxPerStageDescriptorSamplers)
    return std::unexpected(RendererException::format("Vulkan effect '{}' exceeds device uniform/sampler limits", name));
  for (auto const& [texture, framebuffer] : effect->inputs)
    if (!effect->textures.contains(texture))
      return std::unexpected(RendererException::format("Vulkan effect '{}' framebuffer input '{}' has no effectTextures entry", name, texture));
  auto vertex = compileShader(device, name + ".vertex", vertexSource->get(), declarations, shaderc_vertex_shader);
  if (!vertex) return std::unexpected(std::move(vertex.error()));
  effect->vertex = *vertex;
  auto fragment = compileShader(device, name + ".fragment", fragmentSource->get(), declarations, shaderc_fragment_shader);
  if (!fragment) return std::unexpected(std::move(fragment.error()));
  effect->fragment = *fragment;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
  for (std::uint32_t i = 1; i < binding; ++i)
    bindings.push_back({i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
  VkDescriptorSetLayoutCreateInfo descriptors{};
  descriptors.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptors.bindingCount = bindings.size(); descriptors.pBindings = bindings.data();
  VK_REQUIRE(vkCreateDescriptorSetLayout(device->logical, &descriptors, nullptr, &effect->descriptors));
  VkPipelineLayoutCreateInfo layout{};
  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.setLayoutCount = 1; layout.pSetLayouts = &effect->descriptors;
  VK_REQUIRE(vkCreatePipelineLayout(device->logical, &layout, nullptr, &effect->layout));
  RENDER_REQUIRE(flush());
  endRendering();
  effects[name] = std::move(effect); currentEffect = name;
  output = effects[name]->output;
  return {};
}

Result<std::map<String, Framebuffer>> VulkanRenderer::State::createTargets(Json const& definition) {
  JSON_REQUIRE(definitions, definition.get_object("frameBuffers", {}));
  std::map<String, Framebuffer> replacements;
  for (auto const& [name, value] : *definitions) {
    Framebuffer framebuffer;
    framebuffer.config = value;
    JSON_REQUIRE(clear, value.get_bool("clear", true)); framebuffer.clear = *clear;
    JSON_REQUIRE(alpha, value.get_bool("alpha", false)); framebuffer.alpha = *alpha;
    JSON_REQUIRE(doubleBuffered, value.get_bool("double", false)); framebuffer.doubleBuffered = *doubleBuffered;
    JSON_REQUIRE(divisor, value.get_uint("sizeDiv", 1));
    if (!*divisor || *divisor > std::numeric_limits<unsigned>::max())
      return std::unexpected(RendererException("Vulkan framebuffer sizeDiv must be a positive unsigned integer"));
    framebuffer.divisor = *divisor;
    JSON_REQUIRE(fixedSize, value.opt_array("size"));
    if (*fixedSize) {
      JSON_REQUIRE(parsedSize, json_to_vec2u(**fixedSize));
      if (!(*parsedSize)[0] || !(*parsedSize)[1]) return std::unexpected(RendererException("Vulkan fixed framebuffer dimensions must be positive"));
      framebuffer.fixedSize = *parsedSize;
    }
    auto dimensions = framebuffer.fixedSize.value_or(Vec2U(std::max(1u, size[0] / framebuffer.divisor), std::max(1u, size[1] / framebuffer.divisor)));
    JSON_REQUIRE(hdrName, value.get_string("hdr", "Disabled"));
    JSON_REQUIRE(hdrMode, hdrName->to_lower());
    bool floating = false;
    if (*hdrMode == "enabled") floating = true;
    else if (*hdrMode == "fromsetting") floating = hdr;
    else if (*hdrMode != "disabled") return std::unexpected(RendererException::format("Unknown Vulkan framebuffer HDR mode '{}'", *hdrName));
    // Every scene/postprocessing target must preserve super-white values,
    // including targets whose legacy configuration explicitly disabled floats.
    floating |= outputSettings.mode != RenderOutputMode::Legacy;
    JSON_REQUIRE(sampleCount, value.get_uint("multisample", samples));
    if (*sampleCount > 64 || (*sampleCount && (*sampleCount & (*sampleCount - 1))))
      return std::unexpected(RendererException("Vulkan framebuffer multisample must be 0 or a power of two up to 64"));
    if (*sampleCount > 1 && !device->sampleShading)
      return std::unexpected(RendererException("Vulkan device does not support per-sample shading"));
    auto sampleBits = static_cast<VkSampleCountFlagBits>(std::max<std::uint64_t>(*sampleCount, 1));
    JSON_REQUIRE(addressingName, value.get_string("textureAddressing", "clamp"));
    JSON_REQUIRE(filteringName, value.get_string("textureFiltering", "nearest"));
    JSON_REQUIRE(addressing, enum_value<TextureAddressingNames>(*addressingName));
    JSON_REQUIRE(filtering, enum_value<TextureFilteringNames>(*filteringName));
    for (unsigned i = 0; i < (framebuffer.doubleBuffered ? 2u : 1u); ++i) {
      auto target = makeTarget(dimensions, floating ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM, sampleBits, *addressing, *filtering, framebuffer.alpha);
      if (!target) return std::unexpected(std::move(target.error()));
      framebuffer.targets[i] = std::move(*target);
    }
    replacements.emplace(name, std::move(framebuffer));
  }
  return replacements;
}

void VulkanRenderer::State::installTargets(Json const& definition, std::map<String, Framebuffer> replacements) {
  endRendering();
  framebuffers = std::move(replacements);
  // Inputs hold concrete image generations; refresh them on the next effect
  // selection instead of retaining resized framebuffer generations forever.
  for (auto& [name, effect] : effects)
    for (auto const& [texture, framebuffer] : effect->inputs) effect->textures.at(texture).image.reset();
  config = definition;
  currentEffect = "internal"; output.reset();
}

Result<void> VulkanRenderer::State::rebuildTargets(Json const& definition) {
  auto replacements = createTargets(definition);
  if (!replacements) return std::unexpected(std::move(replacements.error()));
  installTargets(definition, std::move(*replacements));
  return {};
}

void VulkanRenderer::State::endRendering() {
  if (rendering) { vkCmdEndRendering(active->get().command); rendering = false; }
}

void VulkanRenderer::State::barrier(GpuImage& image, VkImageLayout layout) {
  endRendering();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = image.layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  barrier.oldLayout = image.layout; barrier.newLayout = layout;
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image.handle; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(active->get().command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  image.layout = layout;
}

Result<void> VulkanRenderer::State::upload(std::shared_ptr<GpuImage> const& image) {
  auto& frame = active->get();
  if (!image->pending.empty()) {
    barrier(*image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    for (auto const& patch : image->pending) {
      auto slice = frame.allocate(patch.bytes.size(), 16);
      if (!slice) return std::unexpected(std::move(slice.error()));
      std::memcpy(static_cast<std::uint8_t*>(slice->buffer->mapped) + slice->offset, patch.bytes.data(), patch.bytes.size());
      VkBufferImageCopy copy{};
      copy.bufferOffset = slice->offset; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      copy.imageOffset = {static_cast<std::int32_t>(patch.offset[0]), static_cast<std::int32_t>(patch.offset[1]), 0};
      copy.imageExtent = {patch.size[0], patch.size[1], 1};
      vkCmdCopyBufferToImage(frame.command, slice->buffer->handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }
    image->pending.clear();
  }
  if (image->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    barrier(*image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  frame.imagePins.push_back(image);
  return {};
}

Result<void> VulkanRenderer::State::replacePixels(std::shared_ptr<GpuImage> const& image, ImageView const& pixels) {
  auto stride = image->format == VK_FORMAT_R32G32B32A32_SFLOAT ? 16u : 4u;
  auto bytes = std::size_t(image->size[0]) * image->size[1] * stride;
  if (!active) {
    Upload replacement;
    if (pixels.empty()) replacement = {Vec2U(0, 0), image->size, std::vector<std::uint8_t>(bytes, 0)};
    else {
      auto packed = packPixels(pixels);
      if (!packed) return std::unexpected(std::move(packed.error()));
      replacement = std::move(*packed);
    }
    image->pending.clear(); image->pending.push_back(std::move(replacement));
    return {};
  }
  auto& frame = active->get();
  auto slice = frame.allocate(bytes, 16);
  if (!slice) return std::unexpected(std::move(slice.error()));
  auto storage = std::span(static_cast<std::uint8_t*>(slice->buffer->mapped) + slice->offset, bytes);
  if (pixels.empty()) std::ranges::fill(storage, 0);
  else RENDER_REQUIRE(copyPixels(pixels, storage));
  image->pending.clear();
  barrier(*image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VkBufferImageCopy copy{};
  copy.bufferOffset = slice->offset; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = {image->size[0], image->size[1], 1};
  vkCmdCopyBufferToImage(frame.command, slice->buffer->handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  barrier(*image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  frame.imagePins.push_back(image);
  return {};
}

Result<std::reference_wrapper<Target>> VulkanRenderer::State::target() {
  if (copyTarget) return std::ref(*copyTarget);
  if (!output) return std::ref(screen);
  auto found = framebuffers.find(*output);
  if (found == framebuffers.end())
    return std::unexpected(RendererException::format("Vulkan framebuffer '{}' does not exist", *output));
  return std::ref(found->second.targets[found->second.current]);
}

Result<void> VulkanRenderer::State::clear(Target const& target, bool alpha) {
  VkClearColorValue black{};
  black.float32[3] = 1;
  VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  // vkCmdClearColorImage requires single-sample images. Multisample targets
  // are initialized through a dynamic rendering clear and automatic resolve.
  if (target.color->samples == VK_SAMPLE_COUNT_1_BIT) {
    barrier(*target.color, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdClearColorImage(active->get().command, target.color->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
  } else {
    barrier(*target.color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    barrier(*target.resolve, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = target.color->view; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT; color.resolveImageView = target.resolve->view; color.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE; color.clearValue.color = black;
    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea.extent = {target.color->size[0], target.color->size[1]}; info.layerCount = 1; info.colorAttachmentCount = 1; info.pColorAttachments = &color;
    vkCmdBeginRendering(active->get().command, &info); vkCmdEndRendering(active->get().command);
  }
  active->get().imagePins.push_back(target.color);
  active->get().imagePins.push_back(target.resolve);
  (void)alpha; // Clear color alpha matches the existing renderer for both modes.
  return {};
}

Result<void> VulkanRenderer::State::beginRendering(Target const& target, bool alpha) {
  if (target.color->layout == VK_IMAGE_LAYOUT_UNDEFINED) RENDER_REQUIRE(clear(target, alpha));
  barrier(*target.color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo color{};
  color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color.imageView = target.color->view; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (target.color != target.resolve) {
    barrier(*target.resolve, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    color.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT; color.resolveImageView = target.resolve->view; color.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  VkRenderingInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  info.renderArea.extent = {target.color->size[0], target.color->size[1]};
  info.layerCount = 1; info.colorAttachmentCount = 1; info.pColorAttachments = &color;
  vkCmdBeginRendering(active->get().command, &info); rendering = true;
  active->get().imagePins.push_back(target.color);
  active->get().imagePins.push_back(target.resolve);
  return {};
}

Result<VkPipeline> VulkanRenderer::State::pipeline(Effect& effect, PipelineKey key) {
  if (auto found = effect.pipelines.find(key); found != effect.pipelines.end()) return found->second;
  std::array<VkPipelineShaderStageCreateInfo, 2> shaders{};
  shaders[0].sType = shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT; shaders[0].module = effect.vertex; shaders[0].pName = "main";
  shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; shaders[1].module = effect.fragment; shaders[1].pName = "main";
  VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
  std::array<VkVertexInputAttributeDescription, 4> attributes{{
    {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position)}, {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
    {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vertex, color)}, {3, 0, VK_FORMAT_R32_SINT, offsetof(Vertex, flags)}
  }};
  VkPipelineVertexInputStateCreateInfo vertex{};
  vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex.vertexBindingDescriptionCount = 1; vertex.pVertexBindingDescriptions = &binding;
  vertex.vertexAttributeDescriptionCount = attributes.size(); vertex.pVertexAttributeDescriptions = attributes.data();
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth = 1;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = key.samples; multisample.sampleShadingEnable = key.samples != VK_SAMPLE_COUNT_1_BIT; multisample.minSampleShading = 1;
  VkPipelineColorBlendAttachmentState attachment{};
  attachment.blendEnable = key.blend;
  attachment.srcColorBlendFactor = attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  attachment.dstColorBlendFactor = attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  attachment.colorBlendOp = attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | (key.alpha ? VK_COLOR_COMPONENT_A_BIT : 0);
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1; blend.pAttachments = &attachment;
  std::array<VkDynamicState, 2> states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = states.size(); dynamic.pDynamicStates = states.data();
  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = 1; rendering.pColorAttachmentFormats = &key.format;
  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = &rendering; info.stageCount = shaders.size(); info.pStages = shaders.data();
  info.pVertexInputState = &vertex; info.pInputAssemblyState = &assembly; info.pViewportState = &viewport;
  info.pRasterizationState = &raster; info.pMultisampleState = &multisample; info.pColorBlendState = &blend;
  info.pDynamicState = &dynamic; info.layout = effect.layout;
  VkPipeline handle;
  VK_REQUIRE(vkCreateGraphicsPipelines(device->logical, VK_NULL_HANDLE, 1, &info, nullptr, &handle));
  effect.pipelines.emplace(key, handle);
  return handle;
}

Result<void> VulkanRenderer::State::draw(std::shared_ptr<Geometry> const& geometry, Mat3F const& transform, bool retained) {
  if (geometry->vertices.empty() || !active) return {};
  auto effect = effects.at(transforming ? String("internalOutput") : copying ? String("internalCopy") : currentEffect);
  auto destination = target();
  if (!destination) return std::unexpected(std::move(destination.error()));
  auto& target = destination->get();
  bool alpha = !target.resolve->opaque;
  auto selectedPipeline = pipeline(*effect, {target.color->format, target.color->samples, alpha, !copying});
  if (!selectedPipeline) return std::unexpected(std::move(selectedPipeline.error()));
  auto& frame = active->get();
  RingSlice vertices;
  auto bytes = std::span(geometry->vertices).size_bytes();
  if (retained) {
    if (!geometry->buffer || geometry->buffer->size < bytes) {
      auto buffer = makeBuffer(device, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
      if (!buffer) return std::unexpected(std::move(buffer.error()));
      geometry->buffer = std::move(*buffer);
    }
    if (geometry->dirty) {
      std::memcpy(geometry->buffer->mapped, geometry->vertices.data(), bytes);
      geometry->dirty = false;
    }
    vertices.buffer = geometry->buffer;
    frame.geometryPins.push_back(geometry);
  } else {
    auto slice = frame.allocate(bytes, 16);
    if (!slice) return std::unexpected(std::move(slice.error()));
    vertices = std::move(*slice);
    std::memcpy(static_cast<std::uint8_t*>(vertices.buffer->mapped) + vertices.offset, geometry->vertices.data(), bytes);
  }
  frame.texturePins.insert(frame.texturePins.end(), geometry->pins.begin(), geometry->pins.end());
  frame.effectPins.push_back(effect);
  auto whiteImage = as_ref<VkTexture>(*white)->get().image;
  for (auto const& batch : geometry->batches) {
    auto& images = sampledImages;
    images.assign(4, whiteImage);
    if (effect->includeTextures)
      for (std::size_t i = 0; i < batch.textureCount; ++i) images[i] = batch.images[i];
    for (auto const& [name, texture] : effect->textures) images.push_back(texture.image ? texture.image : whiteImage);
    for (auto const& image : images) {
      if (image == target.color || image == target.resolve)
        return std::unexpected(RendererException("Vulkan framebuffer feedback requires ping-pong images"));
      RENDER_REQUIRE(upload(image));
    }
    auto uniformSlice = frame.allocate(effect->uniformSize, std::max<VkDeviceSize>(16, device->properties.limits.minUniformBufferOffsetAlignment));
    if (!uniformSlice) return std::unexpected(std::move(uniformSlice.error()));
    auto storage = std::span(static_cast<std::uint8_t*>(uniformSlice->buffer->mapped) + uniformSlice->offset, effect->uniformSize);
    std::ranges::fill(storage, 0);
    auto put = [&](std::size_t offset, auto const& value) { std::memcpy(storage.data() + offset, &value, sizeof(value)); };
    put(0, Vec2F(target.color->size));
    for (unsigned column = 0; column < 3; ++column)
      for (unsigned row = 0; row < 3; ++row) put(16 + column * 16 + row * 4, transform[row][column]);
    for (unsigned i = 0; i < 4; ++i) put(64 + i * 16, Vec2F(images[i]->size));
    bool linearInputs = !copying && outputSettings.mode != RenderOutputMode::Legacy
        && (output.has_value() || outputSettings.mode == RenderOutputMode::Hdr10);
    put(effect->outputOffset, Vec4F(linearInputs ? 1.0f : 0.0f, 0, 0, 0));
    put(effect->outputOffset + 16, outputTransform);
    for (auto const& [name, parameter] : effect->parameters) {
      if (!parameter.value) continue;
      std::visit([&](auto const& value) {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<T, bool>) put(parameter.offset, std::int32_t(value));
        else put(parameter.offset, value);
      }, *parameter.value);
    }
    std::size_t textureIndex = 4;
    for (auto const& [name, texture] : effect->textures) {
      if (texture.sizeOffset) put(texture.sizeOffset, Vec2F(images[textureIndex]->size));
      ++textureIndex;
    }
    auto descriptor = frame.descriptor(effect->descriptors, images.size());
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    VkDescriptorBufferInfo uniformInfo{uniformSlice->buffer->handle, uniformSlice->offset, effect->uniformSize};
    imageInfos.clear(); imageInfos.reserve(images.size());
    for (std::size_t i = 0; i < images.size(); ++i) {
      auto const& image = images[i];
      auto view = image->sampledView ? image->sampledView : image->view;
      if (linearInputs && i < 4 && image->format == VK_FORMAT_R8G8B8A8_UNORM) {
        if (!image->srgbView) {
          VkImageViewCreateInfo info{};
          info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
          info.image = image->handle; info.viewType = VK_IMAGE_VIEW_TYPE_2D; info.format = VK_FORMAT_R8G8B8A8_SRGB;
          info.components.a = image->opaque ? VK_COMPONENT_SWIZZLE_ONE : VK_COMPONENT_SWIZZLE_IDENTITY;
          info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
          VK_REQUIRE(vkCreateImageView(device->logical, &info, nullptr, &image->srgbView));
        }
        view = image->srgbView;
      }
      imageInfos.push_back({copying ? whiteImage->sampler : image->sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }
    auto& writes = descriptorWrites;
    writes.clear(); writes.reserve(images.size() + 1);
    VkWriteDescriptorSet uniformWrite{};
    uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformWrite.dstSet = *descriptor; uniformWrite.dstBinding = 0; uniformWrite.descriptorCount = 1;
    uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; uniformWrite.pBufferInfo = &uniformInfo;
    writes.push_back(uniformWrite);
    for (std::uint32_t i = 0; i < images.size(); ++i) {
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = *descriptor; write.dstBinding = i + 1; write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &imageInfos[i];
      writes.push_back(write);
    }
    vkUpdateDescriptorSets(device->logical, writes.size(), writes.data(), 0, nullptr);
    RENDER_REQUIRE(beginRendering(target, alpha));
    VkViewport viewport{0, 0, float(target.color->size[0]), float(target.color->size[1]), 0, 1};
    vkCmdSetViewport(frame.command, 0, 1, &viewport);
    VkRect2D clip{{0, 0}, {target.color->size[0], target.color->size[1]}};
    if (scissor) {
      auto left = std::clamp(scissor->x_min(), 0, static_cast<int>(target.color->size[0]));
      auto bottom = std::clamp(scissor->y_min(), 0, static_cast<int>(target.color->size[1]));
      auto right = std::clamp(scissor->x_max(), left, static_cast<int>(target.color->size[0]));
      auto top = std::clamp(scissor->y_max(), bottom, static_cast<int>(target.color->size[1]));
      clip = {{left, bottom}, {static_cast<unsigned>(right - left), static_cast<unsigned>(top - bottom)}};
    }
    vkCmdSetScissor(frame.command, 0, 1, &clip);
    vkCmdBindPipeline(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS, *selectedPipeline);
    vkCmdBindDescriptorSets(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS, effect->layout, 0, 1, &*descriptor, 0, nullptr);
    vkCmdBindVertexBuffers(frame.command, 0, 1, &vertices.buffer->handle, &vertices.offset);
    vkCmdDraw(frame.command, batch.count, 1, batch.first, 0);
  }
  sampledImages.clear();
  return {};
}

Result<void> VulkanRenderer::State::flush(Mat3F const& transform) {
  RENDER_REQUIRE(immediate.append(primitives));
  primitives.clear();
  auto result = draw(immediate.geometry, transform, false);
  immediate.geometry->vertices.clear(); immediate.geometry->batches.clear(); immediate.geometry->pins.clear();
  return result;
}

Result<void> VulkanRenderer::State::blit(Target const& source, Target const& destination, bool flip) {
  if (!active || source.resolve == destination.resolve) return {};
  // Render a copy, rather than transfer into an MSAA image (which Vulkan
  // forbids). This also handles HDR conversion, scaling and MSAA resolves.
  auto const& geometry = copyGeometry;
  geometry->batches.clear();
  Vec2F size(destination.color->size), uv(source.resolve->size);
  Vec4B color(255, 255, 255, 255);
  Vertex a{{0, 0}, {0, 0}, color, 0}, b{{size[0], 0}, {uv[0], 0}, color, 0};
  Vertex c{size, uv, color, 0}, d{{0, size[1]}, {0, uv[1]}, color, 0};
  if (flip) { a.uv[1] = b.uv[1] = uv[1]; c.uv[1] = d.uv[1] = 0; }
  geometry->vertices = {a, b, c, a, c, d};
  Batch batch; batch.images[0] = source.resolve; batch.textureCount = 1; batch.count = 6;
  geometry->batches.push_back(std::move(batch));
  struct Restore {
    State& state;
    ~Restore() { state.endRendering(); state.copyTarget.reset(); state.copying = false; }
  } restore{*this};
  copyTarget = destination; copying = true;
  return draw(geometry, Mat3F::identity(), false);
}

Result<void> VulkanRenderer::State::transform(Target const& source, Target const& destination, Vec4F const& parameters, bool flip) {
  struct Restore {
    State& state;
    std::optional<RectI> scissor;
    ~Restore() {
      state.transforming = false; state.outputTransform = Vec4F(0, 0, 0, 0);
      state.scissor = std::move(scissor);
    }
  } restore{*this, std::exchange(scissor, {})};
  transforming = true; outputTransform = parameters;
  return blit(source, destination, flip);
}

Result<void> VulkanRenderer::State::chooseEffect(String const& name) {
  auto effect = effects.at(name);
  std::optional<std::reference_wrapper<Framebuffer>> destination;
  if (effect->output) {
    auto found = framebuffers.find(*effect->output);
    if (found == framebuffers.end()) return std::unexpected(RendererException::format("Vulkan framebuffer '{}' does not exist", *effect->output));
    destination = std::ref(found->second);
  }
  for (auto const& [texture, framebuffer] : effect->inputs)
    if (!framebuffers.contains(framebuffer))
      return std::unexpected(RendererException::format("Vulkan input framebuffer '{}' does not exist", framebuffer));
  if (effect->blit && !framebuffers.contains(*effect->blit))
    return std::unexpected(RendererException::format("Vulkan blit framebuffer '{}' does not exist", *effect->blit));
  RENDER_REQUIRE(flush());
  endRendering();
  if (destination && effect->pingPong) {
    auto& framebuffer = destination->get();
    if (!framebuffer.doubleBuffered) {
      auto const& existing = framebuffer.targets[framebuffer.current];
      auto alternate = makeTarget(existing.color->size, existing.color->format, existing.color->samples, existing.color->addressing, existing.color->filtering, framebuffer.alpha);
      if (!alternate) return std::unexpected(std::move(alternate.error()));
      framebuffer.targets[1 - framebuffer.current] = std::move(*alternate); framebuffer.doubleBuffered = true;
      if (active) RENDER_REQUIRE(clear(framebuffer.targets[1 - framebuffer.current], framebuffer.alpha));
    }
    framebuffer.current = 1 - framebuffer.current;
  }
  for (auto const& [texture, framebufferName] : effect->inputs) {
    auto& framebuffer = framebuffers.at(framebufferName);
    auto inputIndex = effect->pingPong && effect->output && framebufferName == *effect->output ? 1 - framebuffer.current : framebuffer.current;
    effect->textures.at(texture).image = framebuffer.targets[inputIndex].resolve;
  }
  currentEffect = name; output = effect->output;
  if (auto found = effect->parameters.find("vertexRounding"); found != effect->parameters.end() && !found->second.scriptable)
    found->second.value = RenderEffectParameter(samples > 0);
  if (effect->blit && active) {
    auto& framebuffer = framebuffers.at(*effect->blit);
    auto inputIndex = effect->pingPong && effect->output && *effect->blit == *effect->output ? 1 - framebuffer.current : framebuffer.current;
    auto destinationTarget = target();
    if (!destinationTarget) return std::unexpected(std::move(destinationTarget.error()));
    if (outputSettings.mode != RenderOutputMode::Legacy && name == "interface" && *effect->blit == "main" && !effect->output) {
      bool sdr = outputSettings.mode == RenderOutputMode::Sdr;
      RENDER_REQUIRE(transform(framebuffer.targets[inputIndex], destinationTarget->get(),
          Vec4F(outputSettings.exposure, sdr ? 1.0f : 0.0f, sdr ? 1.0f : outputSettings.peakNits / outputSettings.paperWhiteNits, 1)));
    } else {
      RENDER_REQUIRE(blit(framebuffer.targets[inputIndex], destinationTarget->get()));
    }
  }
  return {};
}

VulkanRenderer::VulkanRenderer(std::unique_ptr<State> state) : m_state(std::move(state)) {}
VulkanRenderer::~VulkanRenderer() = default;

Result<std::shared_ptr<VulkanRenderer>> VulkanRenderer::create(Logger& logger, SDL_Window& window, bool vsync) {
  auto state = std::make_unique<State>(logger, window, vsync);
  auto initialized = state->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return std::shared_ptr<VulkanRenderer>(new VulkanRenderer(std::move(state)));
}
String VulkanRenderer::rendererId() const { return "Vulkan"; }
Vec2U VulkanRenderer::screenSize() const { return m_state->size; }
Result<void> VulkanRenderer::initializeGui() { return m_state->initializeGui(); }
void VulkanRenderer::newGuiFrame() { if (m_state->gui) ImGui_ImplVulkan_NewFrame(); }
void VulkanRenderer::shutdownGui() {
  if (!m_state->gui) return;
  vkDeviceWaitIdle(m_state->device->logical);
  ImGui_ImplVulkan_Shutdown(); m_state->gui = false;
}
Result<void> VulkanRenderer::setVSyncEnabled(bool enabled) {
  if (m_state->vsync == enabled) return {};
  if (!enabled) {
    std::uint32_t count = 0;
    VK_REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(m_state->device->physical, m_state->device->surface, &count, nullptr));
    std::vector<VkPresentModeKHR> modes(count);
    VK_REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(m_state->device->physical, m_state->device->surface, &count, modes.data()));
    if (std::ranges::find(modes, VK_PRESENT_MODE_IMMEDIATE_KHR) == modes.end() && std::ranges::find(modes, VK_PRESENT_MODE_MAILBOX_KHR) == modes.end())
      return std::unexpected(RendererException("Vulkan surface cannot disable VSync: no immediate or mailbox presentation mode"));
  }
  m_state->vsync = enabled; m_state->dirtySwapchain = true;
  return {};
}
Result<void> VulkanRenderer::loadConfig(Json const& config) {
  RENDER_REQUIRE(m_state->flush());
  return m_state->rebuildTargets(config);
}
Result<void> VulkanRenderer::loadEffectConfig(String const& name, Json const& config, StringMap<String> const& shaders) {
  return m_state->loadEffect(name, config, shaders);
}
Result<void> VulkanRenderer::setEffectParameter(String const& name, RenderEffectParameter const& value) {
  auto effect = m_state->effects.at(m_state->currentEffect);
  auto found = effect->parameters.find(name);
  if (found == effect->parameters.end() || found->second.scriptable) return {};
  if (found->second.type != value.index())
    return std::unexpected(RendererException::format("VulkanRenderer::setEffectParameter '{}' parameter type mismatch", name));
  if (found->second.value && *found->second.value == value) return {};
  RENDER_REQUIRE(m_state->flush());
  found->second.value = value;
  return {};
}
Result<void> VulkanRenderer::setEffectScriptableParameter(String const& effectName, String const& name, RenderEffectParameter const& value) {
  auto effect = m_state->effects.find(effectName);
  if (effect == m_state->effects.end()) return {};
  auto found = effect->second->parameters.find(name);
  if (found == effect->second->parameters.end() || !found->second.scriptable) return {};
  RenderEffectParameter converted = value;
  if (found->second.type == 1 && std::holds_alternative<float>(value)) {
    auto number = std::get<float>(value);
    // The noinline integer-classification helper preserves this check under
    // fast-math. Use double bounds so INT_MAX does not round up to 2^31.
    if (!is_finite(number) || double(number) < double(std::numeric_limits<int>::min()) || double(number) > double(std::numeric_limits<int>::max()))
      return std::unexpected(RendererException::format("Vulkan scriptable integer '{}' is not a finite int32 value", name));
    converted = static_cast<int>(number);
  }
  if (found->second.type != converted.index())
    return std::unexpected(RendererException::format("VulkanRenderer::setEffectScriptableParameter '{}' parameter type mismatch", name));
  // Preserve ordering even when the selected effect is changed via Lua between
  // buffered immediate commands. Wrong types above never mutate/flush state.
  RENDER_REQUIRE(m_state->flush());
  found->second.value = std::move(converted);
  return {};
}
std::optional<RenderEffectParameter> VulkanRenderer::getEffectScriptableParameter(String const& effectName, String const& name) {
  auto effect = m_state->effects.find(effectName);
  if (effect == m_state->effects.end()) return {};
  auto found = effect->second->parameters.find(name);
  if (found == effect->second->parameters.end() || !found->second.scriptable) return {};
  return found->second.value;
}
Result<void> VulkanRenderer::setEffectTexture(String const& name, ImageView const& pixels) {
  auto effect = m_state->effects.at(m_state->currentEffect);
  auto found = effect->textures.find(name);
  if (found == effect->textures.end()) return {};
  RENDER_REQUIRE(m_state->flush());
  auto& texture = found->second;
  Vec2U dimensions = pixels.empty() ? Vec2U(1, 1) : pixels.size;
  auto format = pixels.format == PixelFormat::RGB_F || pixels.format == PixelFormat::RGBA_F ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
  auto matches = [&](std::shared_ptr<GpuImage> const& image) {
    return image && image->size == dimensions && image->format == format && image->samples == VK_SAMPLE_COUNT_1_BIT && !image->opaque;
  };
  std::erase_if(texture.reusable, [&](auto const& image) { return !matches(image); });
  std::shared_ptr<GpuImage> selected;
  if (matches(texture.image) && texture.image.use_count() == 1) selected = texture.image;
  if (!selected) {
    auto available = std::find_if(texture.reusable.begin(), texture.reusable.end(), [](auto const& image) { return image.use_count() == 1; });
    if (available != texture.reusable.end()) {
      selected = std::move(*available); texture.reusable.erase(available);
    }
  }
  if (!selected) {
    auto image = makeImage(m_state->device, dimensions, format, VK_SAMPLE_COUNT_1_BIT, texture.addressing, texture.filtering);
    if (!image) return std::unexpected(std::move(image.error()));
    selected = std::move(*image);
  }
  RENDER_REQUIRE(m_state->replacePixels(selected, pixels));
  if (texture.image != selected && matches(texture.image)) texture.reusable.push_back(std::move(texture.image));
  texture.image = std::move(selected);
  return {};
}
Result<bool> VulkanRenderer::switchEffectConfig(String const& name) {
  RENDER_REQUIRE(m_state->flush());
  auto effect = m_state->effects.find(name);
  if (effect == m_state->effects.end()) return false;
  if (m_state->currentEffect != name || effect->second->pingPong)
    RENDER_REQUIRE(m_state->chooseEffect(name));
  return true;
}
Result<void> VulkanRenderer::setScissorRect(std::optional<RectI> const& rect) {
  if (rect != m_state->scissor) { RENDER_REQUIRE(m_state->flush()); m_state->scissor = rect; }
  return {};
}
std::optional<RectI> VulkanRenderer::scissorRect() const { return m_state->scissor; }
Result<TexturePtr> VulkanRenderer::createTexture(Image const& pixels, TextureAddressing addressing, TextureFiltering filtering) {
  auto image = imageFromPixels(m_state->device, ImageView(pixels), addressing, filtering);
  if (!image) return std::unexpected(std::move(image.error()));
  auto texture = make_ref<VkTexture>();
  texture->dimensions = pixels.size(); texture->image = std::move(*image);
  return TexturePtr(std::move(texture));
}
void VulkanRenderer::setSizeLimitEnabled(bool enabled) { m_state->sizeLimit = enabled; }
void VulkanRenderer::setMultiTexturingEnabled(bool enabled) { m_state->multi = enabled; m_state->immediate.multi = enabled; }
Result<void> VulkanRenderer::setMultiSampling(unsigned samples) {
  if (m_state->samples == samples) return {};
  if (samples > 64 || (samples && (samples & (samples - 1))))
    return std::unexpected(RendererException("Vulkan multisampling must be zero or a power of two up to 64"));
  if (samples > 1 && (!m_state->device->sampleShading || !(m_state->device->properties.limits.framebufferColorSampleCounts & samples)))
    return std::unexpected(RendererException::format("Vulkan device does not support {} samples with per-sample shading", samples));
  RENDER_REQUIRE(m_state->flush());
  auto previous = std::exchange(m_state->samples, samples);
  auto result = m_state->rebuildTargets(m_state->config);
  if (!result) m_state->samples = previous;
  return result;
}
Result<void> VulkanRenderer::setMainHDR(bool enabled) {
  if (m_state->hdr == enabled) return {};
  RENDER_REQUIRE(m_state->flush());
  auto previous = std::exchange(m_state->hdr, enabled);
  auto result = m_state->rebuildTargets(m_state->config);
  if (!result) m_state->hdr = previous;
  return result;
}
Result<void> VulkanRenderer::setOutputSettings(RenderOutputSettings const& settings) {
  RENDER_REQUIRE(validateRenderOutputSettings(settings));
  auto& state = *m_state;
  if (settings == state.outputSettings) return {};
  if (state.active || state.frameOpen)
    return std::unexpected(RendererException("Change Vulkan output settings between frames"));
  if (!state.size[0] || !state.size[1])
    return std::unexpected(RendererException("Restore the Vulkan window before changing display output settings"));
  auto previous = std::exchange(state.outputSettings, settings);
  if (previous.mode == settings.mode) {
    if (previous.peakNits != settings.peakNits) state.updateHdrMetadata();
    return {};
  }
  bool hadSwapchain = state.swapchain != VK_NULL_HANDLE;
  bool hadGui = state.gui;
  auto result = state.recreateSwapchain();
  if (!result) {
    state.outputSettings = previous;
    // A failure before retirement leaves the old output intact. Afterwards,
    // rollback needs a fresh swapchain, never another acquire of the retired one.
    if (hadSwapchain && !state.swapchain) {
      auto restored = state.recreateSwapchain();
      if (restored && hadGui && !state.gui)
        restored = state.initializeGui();
      if (!restored)
        return std::unexpected(RendererException::format("Vulkan output change failed: {}; restoring the previous output also failed: {}", result.error().what(), restored.error().what()));
    }
  }
  return result;
}
Result<std::shared_ptr<TextureGroup>> VulkanRenderer::createTextureGroup(TextureGroupSize size, TextureFiltering filtering) {
  auto maximum = m_state->device->properties.limits.maxImageDimension2D;
  if (size == TextureGroupSize::Large && (m_state->sizeLimit || maximum < 4096)) size = TextureGroupSize::Medium;
  auto cells = size == TextureGroupSize::Large ? 256u : size == TextureGroupSize::Medium ? 128u : 64u;
  cells = std::min(cells, maximum / 16);
  if (!cells) return std::unexpected(RendererException("Vulkan device cannot allocate texture atlas cells"));
  auto group = std::make_shared<VkTextureGroup>();
  group->atlas = std::make_shared<Atlas>(m_state->device, cells, filtering);
  return std::shared_ptr<TextureGroup>(std::move(group));
}
Result<std::shared_ptr<RenderBuffer>> VulkanRenderer::createRenderBuffer() {
  auto buffer = std::make_shared<VkRenderBuffer>();
  buffer->device = m_state->device; buffer->white = m_state->white; buffer->multi = m_state->multi;
  return std::shared_ptr<RenderBuffer>(std::move(buffer));
}
List<RenderPrimitive>& VulkanRenderer::immediatePrimitives() { return m_state->primitives; }
void VulkanRenderer::render(RenderPrimitive primitive) { m_state->primitives.push_back(std::move(primitive)); }
Result<void> VulkanRenderer::appendImmediate(DetachedRenderGeometry const& geometry) {
  RENDER_REQUIRE(m_state->immediate.append(m_state->primitives));
  m_state->primitives.clear();
  return m_state->immediate.append(geometry);
}
Result<void> VulkanRenderer::setRenderBuffer(std::shared_ptr<RenderBuffer> const& input, DetachedRenderGeometry const& geometry) {
  if (!input) return std::unexpected(RendererException("Cannot set a null Vulkan render buffer"));
  auto buffer = as_ref<VkRenderBuffer>(*input);
  if (!buffer || buffer->get().device != m_state->device)
    return std::unexpected(RendererException("Render buffer belongs to another renderer"));
  return buffer->get().replace(geometry);
}
Result<void> VulkanRenderer::renderBuffer(std::shared_ptr<RenderBuffer> const& input, Mat3F const& transform) {
  if (!input) return std::unexpected(RendererException("Cannot draw a null Vulkan render buffer"));
  auto buffer = as_ref<VkRenderBuffer>(*input);
  if (!buffer || buffer->get().device != m_state->device)
    return std::unexpected(RendererException("Render buffer belongs to another renderer"));
  RENDER_REQUIRE(m_state->flush());
  return m_state->draw(buffer->get().geometry, transform, true);
}
Result<void> VulkanRenderer::flush(Mat3F const& transform) { return m_state->flush(transform); }
Result<void> VulkanRenderer::setScreenSize(Vec2U size) {
  if (size == m_state->size) return {};
  RENDER_REQUIRE(m_state->flush());
  m_state->size = size; m_state->dirtySwapchain = true;
  return {};
}
Result<void> VulkanRenderer::startFrame() {
  auto& state = *m_state;
  if (state.frameOpen) return std::unexpected(RendererException("Vulkan frame already started"));
  state.frameOpen = true;
  state.currentEffect = "internal"; state.output.reset();
  if (!state.size[0] || !state.size[1] || (SDL_GetWindowFlags(&state.window) & SDL_WINDOW_MINIMIZED)) return {};
  if (state.dirtySwapchain) RENDER_REQUIRE(state.recreateSwapchain());
  if (!state.swapchain || !state.extent.width || !state.extent.height) return {};
  auto& frame = *state.frames[state.frameIndex];
  RENDER_REQUIRE(frame.reset());
  auto acquired = vkAcquireNextImageKHR(state.device->logical, state.swapchain, UINT64_MAX, frame.acquired, VK_NULL_HANDLE, &state.imageIndex);
  if (acquired == VK_ERROR_OUT_OF_DATE_KHR) { state.dirtySwapchain = true; return {}; }
  if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) return vkError("swapchain acquire", acquired);
  if (acquired == VK_SUBOPTIMAL_KHR) state.dirtySwapchain = true;
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_REQUIRE(vkBeginCommandBuffer(frame.command, &begin));
  state.active = std::ref(frame);
  RENDER_REQUIRE(state.clear(state.screen, true));
  for (auto& [name, framebuffer] : state.framebuffers) {
    for (unsigned i = 0; i < (framebuffer.doubleBuffered ? 2u : 1u); ++i)
      if (framebuffer.clear || framebuffer.targets[i].color->layout == VK_IMAGE_LAYOUT_UNDEFINED)
        RENDER_REQUIRE(state.clear(framebuffer.targets[i], framebuffer.alpha));
  }
  return {};
}
Result<void> VulkanRenderer::finishFrame() {
  RENDER_REQUIRE(m_state->flush());
  m_state->endRendering();
  return {};
}
Result<bool> VulkanRenderer::present(ImDrawData& drawData) {
  auto& state = *m_state;
  if (!state.frameOpen) return std::unexpected(RendererException("Vulkan present without a started frame"));
  state.frameOpen = false;
  if (!state.active) return false;
  auto& frame = state.active->get();
  state.endRendering();
  bool hdrOutput = state.outputSettings.mode == RenderOutputMode::Hdr10;
  if (hdrOutput) {
    // Flip the game's bottom-left image exactly once before top-left ImGui.
    // Both game UI and ImGui are linear relative to the same paper white.
    RENDER_REQUIRE(state.transform(state.screen, state.guiComposition, Vec4F(0, 0, 0, 0), true));
    if (state.gui) {
      RENDER_REQUIRE(state.beginRendering(state.guiComposition, true));
      ImGui_ImplVulkan_RenderDrawData(&drawData, frame.command);
      state.endRendering();
    }
    // PQ is deliberately last: alpha blending into PQ would be incorrect.
    RENDER_REQUIRE(state.transform(state.guiComposition, state.encodedOutput,
        Vec4F(0, state.outputSettings.paperWhiteNits, 0, 2)));
  }
  auto const& presentationImage = hdrOutput ? state.encodedOutput.resolve : state.screen.resolve;
  state.barrier(*presentationImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  auto swapBarrier = [&](VkImageLayout layout, VkAccessFlags access) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = state.swapLayouts[state.imageIndex] == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = access; barrier.oldLayout = state.swapLayouts[state.imageIndex]; barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = state.swapImages[state.imageIndex]; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(frame.command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    state.swapLayouts[state.imageIndex] = layout;
  };
  swapBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
  VkImageBlit copy{};
  copy.srcSubresource = copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  // Internal image row zero is the game's bottom row, exactly like uploaded
  // pixels and OpenGL framebuffer texture coordinates. Flip once for display.
  copy.srcOffsets[0] = {0, hdrOutput ? 0 : static_cast<int>(presentationImage->size[1]), 0};
  copy.srcOffsets[1] = {static_cast<int>(presentationImage->size[0]), hdrOutput ? static_cast<int>(presentationImage->size[1]) : 0, 1};
  copy.dstOffsets[1] = {static_cast<int>(state.extent.width), static_cast<int>(state.extent.height), 1};
  vkCmdBlitImage(frame.command, presentationImage->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      state.swapImages[state.imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy, VK_FILTER_NEAREST);
  if (state.gui && !hdrOutput) {
    swapBarrier(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    VkRenderingAttachmentInfo color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = state.swapViews[state.imageIndex]; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo render{};
    render.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    render.renderArea.extent = state.extent; render.layerCount = 1; render.colorAttachmentCount = 1; render.pColorAttachments = &color;
    vkCmdBeginRendering(frame.command, &render);
    ImGui_ImplVulkan_RenderDrawData(&drawData, frame.command);
    vkCmdEndRendering(frame.command);
  }
  if (guiResult != VK_SUCCESS) return vkError("ImGui rendering", guiResult);
  swapBarrier(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0);
  VK_REQUIRE(vkEndCommandBuffer(frame.command));
  VK_REQUIRE(vkResetFences(state.device->logical, 1, &frame.fence));
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &frame.acquired; submit.pWaitDstStageMask = &waitStage;
  submit.commandBufferCount = 1; submit.pCommandBuffers = &frame.command;
  submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &state.presentSemaphores[state.imageIndex];
  VK_REQUIRE(vkQueueSubmit(state.device->queue, 1, &submit, frame.fence));
  frame.submitted = true;
  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1; present.pWaitSemaphores = &state.presentSemaphores[state.imageIndex];
  present.swapchainCount = 1; present.pSwapchains = &state.swapchain; present.pImageIndices = &state.imageIndex;
  auto result = vkQueuePresentKHR(state.device->queue, &present);
  state.active.reset(); state.frameIndex = (state.frameIndex + 1) % state.frames.size();
  if (result == VK_ERROR_OUT_OF_DATE_KHR) { state.dirtySwapchain = true; return false; }
  bool displayed = state.size[0] && state.size[1] && !(SDL_GetWindowFlags(&state.window) & SDL_WINDOW_MINIMIZED);
  if (result == VK_SUBOPTIMAL_KHR) { state.dirtySwapchain = true; return displayed; }
  if (result != VK_SUCCESS) return vkError("queue presentation", result);
  return displayed;
}

#undef JSON_REQUIRE
#undef RENDER_REQUIRE
#undef VK_REQUIRE
} // namespace Star

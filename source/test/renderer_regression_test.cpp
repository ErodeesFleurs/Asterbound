#include "GL/glew.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "gtest/gtest.h"
#include "StarTestDisplay.hpp"

import std;
import star.format;
import star.renderer;
import star.renderer_opengl;
import star.renderer_vulkan;
import star.logging;
import star.json;
import star.string;
import star.image;
import star.vector;
import star.rect;
import star.matrix3;

using namespace Star;

namespace {
constexpr unsigned Width = 96, Height = 64;
using Pixel = std::array<unsigned char, 4>;

template <typename T>
T checked(std::expected<T, StarException> result) {
  if (!result)
    GTEST_LOG_(FATAL) << result.error().what();
  if constexpr (!std::is_void_v<T>)
    return std::move(*result);
}

Json json(char const* text) {
  return checked(Json::parse(String(std::string_view(text))));
}

// Only active for this executable's explicitly opted-in Vulkan fixture. No
// renderer API or swapchain TRANSFER_SRC usage is needed for pixel readback.
struct VulkanCapture {
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  void* mapped = nullptr;
  unsigned width = 0, height = 0;
  std::atomic<unsigned> errors{0};
  bool captured = false;
  std::string failure;

  bool allocate() {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = Width * Height * 4;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS)
      return false;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(physical, &properties);
    for (unsigned i = 0; i < properties.memoryTypeCount; ++i) {
      constexpr auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      if (!(requirements.memoryTypeBits & (1u << i)) || (properties.memoryTypes[i].propertyFlags & flags) != flags)
        continue;
      VkMemoryAllocateInfo allocation{};
      allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocation.allocationSize = requirements.size;
      allocation.memoryTypeIndex = i;
      return vkAllocateMemory(device, &allocation, nullptr, &memory) == VK_SUCCESS
          && vkBindBufferMemory(device, buffer, memory, 0) == VK_SUCCESS
          && vkMapMemory(device, memory, 0, info.size, 0, &mapped) == VK_SUCCESS;
    }
    return false;
  }

  void release() {
    if (mapped) vkUnmapMemory(device, memory);
    if (buffer) vkDestroyBuffer(device, buffer, nullptr);
    if (memory) vkFreeMemory(device, memory, nullptr);
    mapped = nullptr;
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
  }
};
VulkanCapture* capture = nullptr;

VKAPI_ATTR VkBool32 VKAPI_CALL validationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const* data, void* user) {
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    static_cast<VulkanCapture*>(user)->errors.fetch_add(1, std::memory_order_relaxed);
    std::fprintf(stderr, "Renderer regression Vulkan validation: %s\n", data->pMessage);
  }
  return VK_FALSE;
}
}

extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL __real_vkCreateInstance(VkInstanceCreateInfo const*, VkAllocationCallbacks const*, VkInstance*);
VKAPI_ATTR void VKAPI_CALL __real_vkDestroyInstance(VkInstance, VkAllocationCallbacks const*);
VKAPI_ATTR VkResult VKAPI_CALL __real_vkCreateDevice(VkPhysicalDevice, VkDeviceCreateInfo const*, VkAllocationCallbacks const*, VkDevice*);
VKAPI_ATTR void VKAPI_CALL __real_vkDestroyDevice(VkDevice, VkAllocationCallbacks const*);
VKAPI_ATTR void VKAPI_CALL __real_vkCmdBlitImage(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout,
    uint32_t, VkImageBlit const*, VkFilter);

VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkCreateInstance(VkInstanceCreateInfo const* original,
    VkAllocationCallbacks const* allocator, VkInstance* instance) {
  if (!capture) return __real_vkCreateInstance(original, allocator, instance);
  auto info = *original;
  std::vector<char const*> extensions(original->ppEnabledExtensionNames,
      original->ppEnabledExtensionNames + original->enabledExtensionCount);
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
  std::vector<char const*> layers;
  for (unsigned i = 0; i < original->enabledLayerCount; ++i)
    layers.push_back(original->ppEnabledLayerNames[i]);
  if (std::ranges::none_of(layers, [](auto name) { return std::string_view(name) == "VK_LAYER_KHRONOS_validation"; }))
    layers.push_back("VK_LAYER_KHRONOS_validation");
  VkValidationFeatureEnableEXT synchronization = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
  VkValidationFeaturesEXT validation{};
  validation.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
  validation.enabledValidationFeatureCount = 1;
  validation.pEnabledValidationFeatures = &synchronization;
  validation.pNext = original->pNext;
  VkDebugUtilsMessengerCreateInfoEXT debug{};
  debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug.pNext = &validation;
  debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug.pfnUserCallback = validationMessage;
  debug.pUserData = capture;
  info.pNext = &debug;
  info.enabledExtensionCount = extensions.size();
  info.ppEnabledExtensionNames = extensions.data();
  info.enabledLayerCount = layers.size();
  info.ppEnabledLayerNames = layers.data();
  auto result = __real_vkCreateInstance(&info, allocator, instance);
  if (result == VK_SUCCESS) {
    capture->instance = *instance;
    debug.pNext = nullptr;
    auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!create || create(*instance, &debug, nullptr, &capture->messenger) != VK_SUCCESS)
      capture->failure = "Could not install Vulkan validation callback";
  }
  return result;
}

VKAPI_ATTR void VKAPI_CALL __wrap_vkDestroyInstance(VkInstance instance, VkAllocationCallbacks const* allocator) {
  if (capture && capture->instance == instance && capture->messenger) {
    auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    destroy(instance, capture->messenger, nullptr);
    capture->messenger = VK_NULL_HANDLE;
  }
  __real_vkDestroyInstance(instance, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkCreateDevice(VkPhysicalDevice physical, VkDeviceCreateInfo const* info,
    VkAllocationCallbacks const* allocator, VkDevice* device) {
  auto result = __real_vkCreateDevice(physical, info, allocator, device);
  if (capture && result == VK_SUCCESS) {
    capture->physical = physical;
    capture->device = *device;
  }
  return result;
}

VKAPI_ATTR void VKAPI_CALL __wrap_vkDestroyDevice(VkDevice device, VkAllocationCallbacks const* allocator) {
  if (capture && capture->device == device) {
    vkDeviceWaitIdle(device);
    capture->release();
    capture->device = VK_NULL_HANDLE;
  }
  __real_vkDestroyDevice(device, allocator);
}

VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdBlitImage(VkCommandBuffer command, VkImage source, VkImageLayout sourceLayout,
    VkImage destination, VkImageLayout destinationLayout, uint32_t count, VkImageBlit const* regions, VkFilter filter) {
  __real_vkCmdBlitImage(command, source, sourceLayout, destination, destinationLayout, count, regions, filter);
  if (!capture || count != 1) return;
  // This backend uses vkCmdBlitImage only for its final presentation copy. Read
  // its actual scene source, never the swapchain (which lacks TRANSFER_SRC).
  auto width = std::abs(regions[0].srcOffsets[1].x - regions[0].srcOffsets[0].x);
  auto height = std::abs(regions[0].srcOffsets[1].y - regions[0].srcOffsets[0].y);
  if (sourceLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || width != Width || height != Height) {
    capture->failure = "Unexpected presentation source dimensions/layout";
    return;
  }
  if (!capture->buffer && !capture->allocate()) {
    capture->failure = "Could not allocate coherent Vulkan readback";
    return;
  }
  VkBufferMemoryBarrier reuse{};
  reuse.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  reuse.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
  reuse.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  reuse.srcQueueFamilyIndex = reuse.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  reuse.buffer = capture->buffer;
  reuse.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &reuse, 0, nullptr);
  VkBufferImageCopy copy{};
  copy.imageSubresource = regions[0].srcSubresource;
  copy.imageExtent = {static_cast<unsigned>(width), static_cast<unsigned>(height), 1};
  vkCmdCopyImageToBuffer(command, source, sourceLayout, capture->buffer, 1, &copy);
  VkBufferMemoryBarrier host{};
  host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  host.buffer = capture->buffer;
  host.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
      0, nullptr, 1, &host, 0, nullptr);
  capture->width = width;
  capture->height = height;
  capture->captured = true;
}
}

namespace {
char const* GlVertex = R"GLSL(#version 140
uniform vec2 screenSize;
uniform mat3 vertexTransform;
in vec2 vertexPosition;
in vec4 vertexColor;
out vec2 uv;
out vec4 color;
void main() {
  vec2 p = (vertexTransform * vec3(vertexPosition, 1)).xy;
  gl_Position = vec4(p / screenSize * 2.0 - 1.0, 0, 1);
  uv = vertexPosition / screenSize;
  color = vertexColor;
}
)GLSL";
char const* VkVertex = R"GLSL(#version 450
layout(location=0) in vec2 vertexPosition;
layout(location=2) in vec4 vertexColor;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 color;
void main() {
  vec2 p = (vertexTransform * vec3(vertexPosition, 1)).xy;
  gl_Position = vec4(p / screenSize * 2.0 - 1.0, 0, 1);
  uv = vertexPosition / screenSize;
  color = vertexColor;
}
)GLSL";

class RendererRegression : public testing::Test {
protected:
  Logger logger;
  SDL_Window* window = nullptr;
  SDL_GLContext context = nullptr;
  std::shared_ptr<NativeRenderer> renderer;
  VulkanCapture vk;
  bool video = false;
  virtual bool vulkan() const { return false; }

  void SetUp() override {
    auto lease = std::getenv("ASTERBOUND_RENDER_TEST_DISPLAY");
    if (!lease)
      GTEST_SKIP() << "Requires scripts/ci/linux/with-test-display.py";
    ASSERT_TRUE(matches_test_display(lease)) << "Stale or invalid isolated test display: " << lease;
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "x11", SDL_HINT_OVERRIDE));
    ASSERT_TRUE(SDL_InitSubSystem(SDL_INIT_VIDEO)) << SDL_GetError();
    video = true;
    if (!vulkan()) {
      ASSERT_TRUE(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
      ASSERT_TRUE(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
      ASSERT_TRUE(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
      ASSERT_TRUE(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1));
    }
    window = SDL_CreateWindow("Asterbound owned renderer regression", Width, Height,
        vulkan() ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL);
    ASSERT_NE(window, nullptr) << SDL_GetError();
    if (vulkan()) {
      capture = &vk;
      renderer = checked(VulkanRenderer::create(logger, *window, false));
      ASSERT_TRUE(vk.failure.empty()) << vk.failure;
    } else {
      context = SDL_GL_CreateContext(window);
      ASSERT_NE(context, nullptr) << SDL_GetError();
      ASSERT_TRUE(SDL_GL_MakeCurrent(window, context)) << SDL_GetError();
      renderer = checked(OpenGlRenderer::create(logger));
    }
    checked(renderer->setScreenSize({Width, Height}));
  }

  void TearDown() override {
    renderer.reset(); // Destroy all native resources before SDL/context teardown.
    if (vulkan() && capture) {
      EXPECT_TRUE(vk.failure.empty()) << vk.failure;
      EXPECT_EQ(vk.errors.load(), 0u) << "Including resource teardown validation";
      capture = nullptr;
    }
    if (context) SDL_GL_DestroyContext(context);
    if (window) SDL_DestroyWindow(window);
    if (video) SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }

  void effect(char const* name, Json const& config, char const* body, bool sample = false) {
    std::string fragment = vulkan()
        ? "#version 450\nlayout(location=0) in vec2 uv;\nlayout(location=1) in vec4 color;\nlayout(location=0) out vec4 outColor;\n"
        : "#version 140\nin vec2 uv;\nin vec4 color;\nout vec4 outColor;\n";
    if (sample && !vulkan()) fragment += "uniform sampler2D inputTexture;\nuniform vec2 inputSize;\n";
    fragment += "void main() { ";
    fragment += body;
    fragment += " }\n";
    checked(renderer->loadEffectConfig(String(std::string_view(name)), config,
        {{"vertex", String(std::string_view(vulkan() ? VkVertex : GlVertex))}, {"fragment", String(fragment)}}));
  }

  void solid(char const* name, char const* config) {
    effect(name, json(config), "outColor = color;");
  }

  void select(char const* name) {
    if (!checked(renderer->switchEffectConfig(String(std::string_view(name)))))
      GTEST_LOG_(FATAL) << "Missing regression effect";
  }

  void quad(unsigned width, unsigned height, Pixel color = {255, 255, 255, 255}) {
    renderer->render(RenderQuad(RectF(0, 0, width, height), Vec4B(color[0], color[1], color[2], color[3])));
  }

  void pattern(unsigned width, unsigned height) {
    renderer->render(RenderQuad(RectF(0, 0, width / 2, height), Vec4B(255, 0, 0, 255)));
    renderer->render(RenderQuad(RectF(width / 2, 0, width, height / 2), Vec4B(0, 255, 0, 255)));
    renderer->render(RenderQuad(RectF(width / 2, height / 2, width, height), Vec4B(0, 0, 255, 255)));
    checked(renderer->flush());
  }

  std::vector<Pixel> pixels(unsigned width = Width, unsigned height = Height, bool currentTarget = false) {
    checked(renderer->flush());
    std::vector<Pixel> result(width * height);
    if (vulkan()) {
      checked(renderer->finishFrame());
      ImDrawData empty;
      auto& native = static_cast<VulkanRenderer&>(*renderer);
      EXPECT_TRUE(checked(native.present(empty)));
      EXPECT_EQ(vkDeviceWaitIdle(vk.device), VK_SUCCESS);
      if (!vk.captured || !vk.failure.empty())
        GTEST_LOG_(FATAL) << (vk.failure.empty() ? "No Vulkan presentation capture" : vk.failure);
      std::memcpy(result.data(), vk.mapped, result.size() * sizeof(Pixel));
      vk.captured = false;
    } else {
      GLint previous = 0, draw = 0;
      glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
      glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, currentTarget ? draw : 0);
      glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
      EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
      glBindFramebuffer(GL_READ_FRAMEBUFFER, previous);
    }
    return result;
  }

  void expectPixel(std::vector<Pixel> const& image, unsigned width, unsigned x, unsigned y, Pixel expected, int tolerance = 1) {
    SCOPED_TRACE(testing::Message() << "pixel " << x << ',' << y);
    auto actual = image[y * width + x];
    for (unsigned channel = 0; channel < 3; ++channel)
      EXPECT_LE(std::abs(int(actual[channel]) - int(expected[channel])), tolerance) << "channel " << channel;
  }

  void expectPattern(std::vector<Pixel> const& image, unsigned width = Width, unsigned height = Height) {
    expectPixel(image, width, width / 4, height / 4, {255, 0, 0, 255});
    expectPixel(image, width, width * 3 / 4, height / 4, {0, 255, 0, 255});
    expectPixel(image, width, width * 3 / 4, height * 3 / 4, {0, 0, 255, 255});
    expectPixel(image, width, 1, 1, {255, 0, 0, 255});
    expectPixel(image, width, 1, height - 2, {255, 0, 0, 255});
    expectPixel(image, width, width - 2, 1, {0, 255, 0, 255});
    expectPixel(image, width, width - 2, height - 2, {0, 0, 255, 255});
  }

  void repeatedPasses() {
    checked(renderer->loadConfig(json(R"({"frameBuffers":{"scene":{"double":true}}})")));
    solid("seed", R"({"includeVBTextures":false,"frameBuffer":"scene"})");
    effect("multiply", json(R"({"includeVBTextures":false,"frameBuffer":"scene","frameBufferTextures":[{"framebuffer":"scene","texture":"input"}],"effectTextures":{"input":{"textureUniform":"inputTexture"}}})"),
        "outColor = vec4(texture(inputTexture, uv).rgb * 0.5, 1);", true);
    effect("output", json(R"({"includeVBTextures":false,"frameBufferTextures":[{"framebuffer":"scene","texture":"input"}],"effectTextures":{"input":{"textureUniform":"inputTexture"}}})"),
        "outColor = vec4(texture(inputTexture, uv).rgb, 1);", true);
    for (unsigned passes : {2u, 3u}) {
      SCOPED_TRACE(testing::Message() << passes << " recursive passes");
      checked(renderer->startFrame());
      select("seed");
      quad(Width, Height, {192, 128, 64, 255});
      for (unsigned pass = 0; pass < passes; ++pass) {
        select("multiply"); // Deliberately no intervening effect name.
        quad(Width, Height);
      }
      select("output");
      quad(Width, Height);
      auto image = pixels();
      auto divisor = 1u << passes;
      for (auto point : {std::pair{4u, 4u}, std::pair{Width / 2, Height / 2}, std::pair{Width - 5, Height - 5}})
        expectPixel(image, Width, point.first, point.second,
            {static_cast<unsigned char>(192 / divisor), static_cast<unsigned char>(128 / divisor), static_cast<unsigned char>(64 / divisor), 255}, 2);
      if (!vulkan()) checked(renderer->finishFrame());
    }
  }
};

class OpenGlRendererRegression : public RendererRegression {};
class VulkanRendererRegression : public RendererRegression {
  bool vulkan() const override { return true; }
};

TEST_F(OpenGlRendererRegression, RepeatedPingPongPassesSamplePreviousResult) {
  repeatedPasses();
}

TEST_F(VulkanRendererRegression, RepeatedPingPongPassesSamplePreviousResult) {
  repeatedPasses();
}

TEST_F(VulkanRendererRegression, ReleasedAtlasUploadsDoNotOverwriteReusedCells) {
  // Both equal and cross-shaped padded extents reuse the first allocation's
  // origin. The latter cannot be repaired by deduplicating exact rectangles.
  for (auto sizes : {std::pair{Vec2U(9, 9), Vec2U(9, 9)}, std::pair{Vec2U(13, 7), Vec2U(7, 13)},
         std::pair{Vec2U(7, 13), Vec2U(13, 7)}, std::pair{Vec2U(24, 24), Vec2U(8, 16)}}) {
    auto group = checked(renderer->createTextureGroup(TextureGroupSize::Small, TextureFiltering::Nearest));
    auto abandoned = checked(group->create(Image::filled(sizes.first, Vec4B(255, 0, 0, 255), PixelFormat::RGBA32)));
    abandoned.reset(); // No draw has submitted its pending upload.
    auto replacement = checked(group->create(Image::filled(sizes.second, Vec4B(0, 255, 0, 255), PixelFormat::RGBA32)));
    checked(renderer->startFrame());
    renderer->render(RenderQuad(replacement, RectF(0, 0, Width, Height)));
    auto image = pixels();
    for (auto point : {std::pair{2u, 2u}, std::pair{Width / 2, Height / 2}, std::pair{Width - 3, Height - 3}})
      expectPixel(image, Width, point.first, point.second, {0, 255, 0, 255});
    EXPECT_EQ(vk.errors.load(), 0u) << "Atlas reuse must also be synchronization-validation clean";
  }
}

TEST_F(OpenGlRendererRegression, TargetViewportCoversDividedFixedAndRestoredWindow) {
  checked(renderer->loadConfig(json(R"({"frameBuffers":{"divided":{"sizeDiv":2},"fixed":{"size":[24,16]}}})")));
  solid("divided", R"({"includeVBTextures":false,"frameBuffer":"divided"})");
  solid("fixed", R"({"includeVBTextures":false,"frameBuffer":"fixed"})");
  solid("window", R"({"includeVBTextures":false})");
  checked(renderer->startFrame());
  select("divided");
  pattern(48, 32);
  expectPattern(pixels(48, 32, true), 48, 32);
  // Resize while the divided target is still current; no effect switch may be
  // required to restore its dimensions/viewport/uniforms.
  checked(renderer->setScreenSize({80, 48}));
  pattern(40, 24);
  expectPattern(pixels(40, 24, true), 40, 24);
  select("divided"); // Reselecting the same ordinary effect must stay correct.
  pattern(40, 24);
  expectPattern(pixels(40, 24, true), 40, 24);
  select("fixed");
  pattern(24, 16);
  expectPattern(pixels(24, 16, true), 24, 16);
  select("window");
  pattern(80, 48);
  expectPattern(pixels(80, 48), 80, 48);
  checked(renderer->finishFrame());
}

TEST_F(OpenGlRendererRegression, BlitsScaleBothDirectionsAndRetainMsaaHdr) {
  GLint maximumSamples = 0;
  glGetIntegerv(GL_MAX_SAMPLES, &maximumSamples);
  for (unsigned samples : {0u, 4u}) {
    if (samples && (maximumSamples < 4 || !GLEW_VERSION_4_0)) continue;
    for (bool hdr : {false, true}) {
      for (bool fixed : {false, true}) {
        SCOPED_TRACE(testing::Message() << "samples=" << samples << " hdr=" << hdr << " fixed=" << fixed);
        checked(renderer->setMultiSampling(samples));
        auto config = std::string("{\"frameBuffers\":{\"small\":{")
            + (fixed ? "\"size\":[24,16]," : "\"sizeDiv\":2,")
            + "\"hdr\":\"" + (hdr ? "Enabled" : "Disabled") + "\"},\"large\":{\"hdr\":\""
            + (hdr ? "Enabled" : "Disabled") + "\"}}}";
        checked(renderer->loadConfig(checked(Json::parse(String(config)))));
        solid("small", R"({"includeVBTextures":false,"frameBuffer":"small"})");
        solid("large", R"({"includeVBTextures":false,"frameBuffer":"large"})");
        solid("smallToWindow", R"({"includeVBTextures":false,"blitFrameBuffer":"small"})");
        solid("largeToSmall", R"({"includeVBTextures":false,"frameBuffer":"small","blitFrameBuffer":"large"})");
        solid("largeToWindow", R"({"includeVBTextures":false,"blitFrameBuffer":"large"})");
        checked(renderer->startFrame());
        select("small");
        pattern(fixed ? 24 : 48, fixed ? 16 : 32);
        select("smallToWindow");
        expectPattern(pixels());
        checked(renderer->finishFrame());
        checked(renderer->startFrame());
        select("large");
        pattern(Width, Height);
        select("largeToSmall");
        select("smallToWindow");
        expectPattern(pixels());
        checked(renderer->finishFrame());
        checked(renderer->startFrame());
        select("large");
        pattern(Width, Height);
        select("largeToWindow"); // Original same-sized MSAA resolve path.
        expectPattern(pixels());
        checked(renderer->finishFrame());
      }
    }
  }
}

TEST_F(OpenGlRendererRegression, FramebufferSamplerSizeRefreshesAfterResizeAndSameEffectReselect) {
  checked(renderer->loadConfig(json(R"({"frameBuffers":{"scene":{"sizeDiv":2}}})")));
  solid("seed", R"({"includeVBTextures":false,"frameBuffer":"scene"})");
  auto input = json(R"({"includeVBTextures":false,"frameBufferTextures":[{"framebuffer":"scene","texture":"input"}],"effectTextures":{"input":{"textureUniform":"inputTexture","textureSizeUniform":"inputSize"}}})");
  effect("size", input, "outColor = vec4(inputSize / 128.0, texture(inputTexture, uv).b * 0.001, 1);", true);
  checked(renderer->startFrame());
  select("seed");
  quad(48, 32, {0, 0, 64, 255});
  select("size");
  quad(Width, Height);
  expectPixel(pixels(), Width, 70, 40, {96, 64, 0, 255});
  checked(renderer->setScreenSize({80, 48}));
  select("size"); // Ordinary effect remains current across the resize.
  quad(80, 48);
  expectPixel(pixels(80, 48), 80, 60, 36, {80, 48, 0, 255});
  select("seed");
  quad(40, 24);
  select("size"); // Also cover normal re-entry with an already defined sampler.
  quad(80, 48);
  expectPixel(pixels(80, 48), 80, 60, 36, {80, 48, 0, 255});
  checked(renderer->finishFrame());
}
}

#include "GL/glew.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "gtest/gtest.h"
#include "StarTestDisplay.hpp"

import std;
import star.algorithm;
import star.assets;
import star.asset_path;
import star.buffer;
import star.byte_array;
import star.file;
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
import star.color;
import star.drawable;
import star.drawable_painter;
import star.environment_painter;
import star.game_types;
import star.line;
import star.list;
import star.material_types;
import star.parallax;
import star.poly;
import star.random;
import star.ref_ptr;
import star.root;
import star.sky_render_data;
import star.tile_painter;
import star.world_camera;
import star.world_geometry;
import star.world_render_data;
import star.world_tiles;

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

class OpenGlTileDirectiveRegression : public RendererRegression {
  void TestBody() override {
    RandomService random(0);
    auto directory = checked(File::temporary_directory(random));
    auto cleanup = make_scope_exit([&] { checked(File::remove_all(directory)); });
    auto overlay = File::relative_to(directory, "assets");
    checked(File::create_directory(overlay));
    auto write_asset = [&](std::string_view name, std::string_view text) {
      checked(File::write_file(String(text), File::relative_to(overlay, name)));
    };
    write_asset("preload.config", "[]");
    write_asset("metamaterials.config", "[]");
    write_asset("client.config", R"({"defaultFootstepSound":""})");
    write_asset("rendering.config", R"({
      "backgroundLayerColor":[255,255,255,255], "foregroundLayerColor":[255,255,255,255],
      "liquidDrawLevels":[0,1], "chunkCacheTimeout":600000, "textureTimeout":600000
    })");
    write_asset("liquids.config", R"({
      "liquidEngineParameters":{
        "lateralMoveFactor":0, "spreadOverfillUpFactor":0, "spreadOverfillLateralFactor":0,
        "spreadOverfillDownFactor":0, "pressureEqualizeFactor":0, "pressureMoveFactor":0,
        "maximumPressureLevelImbalance":0, "minimumLivenPressureChange":0,
        "minimumLivenLevelChange":0, "minimumLiquidLevel":0, "interactTransformationLevel":0
      }, "backgroundDrain":0
    })");
    write_asset("directive.material", R"({
      "materialId":1, "materialName":"directive", "category":"test",
      "damageTable":{"damageFactors":{},"damageRecovery":0},
      "renderTemplate":"/directive.config",
      "renderParameters":{"colorDirectives":["?setcolor=ff0000","?setcolor=00ff00"]}
    })");
    write_asset("directive.matmod", R"({
      "modId":1, "modName":"directive",
      "damageTable":{"damageFactors":{},"damageRecovery":0},
      "renderTemplate":"/directive.config",
      "renderParameters":{"colorDirectives":["?setcolor=0000ff","?setcolor=ffff00"],
                          "matches":[["main",[{"matchAllPoints":[],"pieces":[["half",[4,0]]]}]]]}
    })");
    // Both slots sample exactly the same atlas rectangle. The material and
    // mod occupy opposite halves so that neither can hide the other's pixels.
    write_asset("directive.config", R"({
      "texture":"/directive.png", "multiColored":true, "colorVariants":2,
      "representativePiece":"half", "rules":{},
      "pieces":{"half":{"textureSize":[4,8],"texturePosition":[0,0],"colorStride":[0,0]}},
      "matches":[["main",[{"matchAllPoints":[],"pieces":[["half",[0,0]]]}]]]
    })");
    auto png = std::make_shared<Buffer>();
    checked(Image::filled({4, 8}, {255, 255, 255, 255}).write_png(logger, png));
    checked(File::write_file(png->take_data(), File::relative_to(overlay, "directive.png")));

    Root::Settings settings{};
    settings.assetSources = {overlay};
    settings.assetsSettings.asset_time_to_live = 600;
    settings.assetsSettings.allow_remote_assets = false;
    settings.defaultConfiguration = JsonObject{};
    settings.storageDirectory = File::relative_to(directory, "storage");
    settings.logLevel = LogLevel::Error;
    auto root = checked(Root::create(std::move(settings)));

    WorldRenderData data;
    data.geometry = WorldGeometry(128, 128);
    data.tile_min_position = {0, 0};
    checked(data.tiles.set_size({48, 48}));
    for (unsigned x = 0; x != 48; ++x)
      for (unsigned y = 0; y != 48; ++y)
        data.tiles(x, y) = RenderTile{.foreground = empty_material_id, .foregroundMod = no_mod_id,
                                    .background = empty_material_id, .backgroundMod = no_mod_id};
    auto& tile = data.tiles(24, 24);
    tile.foreground = 1;
    tile.foregroundMod = 1;

    WorldCamera camera;
    camera.setWorldGeometry(data.geometry);
    camera.setScreenSize({Width, Height});
    camera.setCenterWorldPosition({24, 24}, true);
    auto material_pixel = camera.worldToScreen({24.25f, 24.5f});
    auto mod_pixel = camera.worldToScreen({24.75f, 24.5f});
    std::array const expected_material{Pixel{255, 0, 0, 255}, Pixel{0, 255, 0, 255}};
    std::array const expected_mod{Pixel{0, 0, 255, 255}, Pixel{255, 255, 0, 255}};
    auto draw = [&](TilePainter& painter, unsigned slot, std::string const& phase) {
      auto location = std::source_location::current();
      testing::ScopedTrace trace(location.file_name(), location.line(), phase);
      tile.foregroundColorVariant = static_cast<MaterialColorVariant>(slot);
      checked(renderer->startFrame());
      select("internal");
      checked(painter.setup(camera, data));
      checked(painter.renderForeground(camera));
      auto frame = pixels();
      auto material_x = static_cast<unsigned>(material_pixel.x());
      auto mod_x = static_cast<unsigned>(mod_pixel.x());
      auto y = static_cast<unsigned>(material_pixel.y());
      auto const& material = frame[y * Width + material_x];
      auto const& mod = frame[y * Width + mod_x];
      RecordProperty(phase, std::format("{:02x}{:02x}{:02x}/{:02x}{:02x}{:02x}",
          material[0], material[1], material[2], mod[0], mod[1], mod[2]));
      expectPixel(frame, Width, material_x, y, expected_material[slot], 0);
      expectPixel(frame, Width, mod_x, y, expected_mod[slot], 0);
      checked(renderer->finishFrame());
      painter.cleanup();
    };

    for (unsigned first : {0u, 1u}) {
      auto order = first == 0 ? std::string("ab_") : std::string("ba_");
      auto second = 1 - first;
      auto painter = checked(TilePainter::create(*root, renderer));
      draw(*painter, first, order + "cold");
      draw(*painter, second, order + "switched");
      draw(*painter, second, order + "warm");
      draw(*painter, first, order + "returned");
      auto fresh = checked(TilePainter::create(*root, renderer));
      draw(*fresh, second, order + "fresh");
    }
  }
};

auto const tile_directive_registration = [] {
  auto location = std::source_location::current();
  return testing::RegisterTest("OpenGlTileDirectiveRegression", "TileDirectiveSlotsAreIndependent",
      nullptr, nullptr, location.file_name(), static_cast<int>(location.line()),
      [] { return std::make_unique<OpenGlTileDirectiveRegression>().release(); });
}();

TEST_F(OpenGlRendererRegression, ParallaxTilesPreserveFrameZeroGeometryAndRetryTextures) {
  RandomService random(0);
  auto directory = checked(File::temporary_directory(random));
  auto cleanup = make_scope_exit([&] { checked(File::remove_all(directory)); });
  auto overlay = File::relative_to(directory, "assets");
  checked(File::create_directory(overlay));
  auto write_asset = [&](String const& name, String const& text) {
    checked(File::write_file(text, File::relative_to(overlay, name)));
  };
  auto write_png = [&](String const& name, Image const& image) {
    auto buffer = std::make_shared<Buffer>();
    checked(image.write_png(logger, buffer));
    checked(File::write_file(buffer->take_data(), File::relative_to(overlay, name)));
  };
  write_asset("preload.config", "[]");
  write_asset("parallax_base.frames", R"({"frameList":{"0":[0,4,16,20],"2":[16,0,40,20]},"aliases":{"1":"0"}})");
  write_asset("parallax_overlay.frames", R"({"frameList":{"2":[0,0,16,16]},"aliases":{"0":"2","1":"2"}})");
  auto base = Image::filled({40, 20}, {0, 255, 255, 255});
  checked(base.fill_rect({16, 0}, {8, 8}, Vec4B(255, 0, 0, 255)));
  checked(base.fill_rect({24, 0}, {16, 8}, Vec4B(0, 255, 0, 255)));
  checked(base.fill_rect({16, 8}, {8, 12}, Vec4B(255, 255, 0, 255)));
  checked(base.fill_rect({24, 8}, {16, 12}, Vec4B(255, 0, 255, 255)));
  write_png("parallax_base.png", base);
  write_png("parallax_overlay.png", Image::filled({16, 16}, {0, 0, 255, 128}));

  Root::Settings settings{};
  settings.assetSources = {overlay};
  settings.assetsSettings.worker_pool_size = 0; // Keep first presentation cold deterministically.
  settings.assetsSettings.asset_time_to_live = 600;
  settings.assetsSettings.allow_remote_assets = false;
  settings.defaultConfiguration = JsonObject{};
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.logLevel = LogLevel::Error;
  auto root = checked(Root::create(std::move(settings)));
  auto assets = checked(root->assets());
  auto painter = checked(EnvironmentPainter::create(*root, renderer));

  WorldCamera camera;
  camera.setScreenSize({Width, Height});
  SkyRenderData sky{};
  sky.dayLength = 120;
  ParallaxLayer layer;
  layer.textures = {"/parallax_base.png", "/parallax_overlay.png"};
  layer.frame_number = 3;
  layer.frame_offset = -1; // Epoch zero selects frame two, not frame zero.
  layer.animation_cycle = 3;
  layer.parallax_value = {1, 1};
  layer.repeat = {false, false};
  layer.unlit = true;
  std::array samples{ParallaxLayerSample{.layer = std::cref(layer), .alpha = 0.5f, .parallax_offset = {}, .speed = {}}};
  auto draw = [&](Vec2F world_position = {}) {
    checked(renderer->startFrame());
    select("internal");
    checked(painter->render_parallax_layers(world_position, camera, samples, sky));
    auto frame = pixels();
    checked(renderer->finishFrame());
    return frame;
  };
  Pixel const black{0, 0, 0, 255};
  expectPixel(draw(), Width, 50, 34, black);
  checked(assets->image(AssetPath("/parallax_base.png:2")));
  expectPixel(draw(), Width, 50, 34, {127, 0, 0, 255});
  checked(assets->image(AssetPath("/parallax_overlay.png:2")));
  auto stacked = draw();
  expectPixel(stacked, Width, 50, 34, {95, 0, 64, 255});
  expectPixel(stacked, Width, 58, 34, {0, 95, 64, 255});
  expectPixel(stacked, Width, 50, 43, {95, 95, 64, 255});
  expectPixel(stacked, Width, 64, 34, black); // Frame two is wider than frame zero.
  expectPixel(draw({100, 0}), Width, 50, 34, black);
  expectPixel(draw({0x1p32f, 0}), Width, 50, 34, black); // Avoid an out-of-range tile index conversion.

  layer.repeat[0] = true;
  auto before_seam = draw({-0.25f, 0});
  auto after_seam = draw({1.75f, 0});
  EXPECT_EQ(before_seam, after_seam); // One image period across the camera seam.
  expectPixel(before_seam, Width, 51, 34, {95, 0, 64, 255});

  layer.repeat = {false, true};
  layer.tile_limit_top = -8;
  auto fractional_top = draw();
  expectPixel(fractional_top, Width, 50, 33, {95, 95, 64, 255}); // Clipped UV starts in the upper half.
  expectPixel(fractional_top, Width, 50, 40, black);
  layer.tile_limit_top = -16;
  auto integer_top = draw();
  expectPixel(integer_top, Width, 50, 33, {95, 0, 64, 255});
  expectPixel(integer_top, Width, 50, 48, black);
  layer.tile_limit_top.reset();
  layer.tile_limit_bottom = 8;
  auto fractional_bottom = draw();
  expectPixel(fractional_bottom, Width, 50, 23, black);
  expectPixel(fractional_bottom, Width, 50, 25, {95, 0, 64, 255});
  layer.tile_limit_bottom = 0; // Keep the complete row with y + 1 equal to the cutoff.
  auto integer_bottom = draw();
  expectPixel(integer_bottom, Width, 50, 14, black);
  expectPixel(integer_bottom, Width, 50, 17, {95, 0, 64, 255});
}

TEST_F(OpenGlRendererRegression, RekeyedLayersPreserveOverlappingPixelsAndEntityOrder) {
  DrawablePainter painter(renderer, {});
  WorldRenderData frame;
  frame.entity_drawables.resize(2);
  FrameLayers<EntityDrawableGroup> routes;
  checked(renderer->loadConfig(json(R"({})")));
  solid("layers", R"({"includeVBTextures":false})");
  for (unsigned vehicle_layer : {10u, 30u, 5u}) {
    frame.prepare_frame();
    auto& first = frame.entity_drawables[0].layers;
    auto& second = frame.entity_drawables[1].layers;
    first.append(20u, Drawable::make_poly(PolyF(RectF(10, 10, 80, 50)), Color::rgb(0, 255, 0)));
    first.append(vehicle_layer, Drawable::make_poly(PolyF(RectF(0, 0, Width, Height)), Color::rgb(255, 0, 0)));
    second.append(20u, Drawable::make_poly(PolyF(RectF(25, 20, 70, 40)), Color::rgb(0, 0, 255)));
    route_entity_drawables(routes, frame.entity_drawables);
    checked(renderer->startFrame());
    select("layers");
    for (auto const& [key, groups] : routes)
      for (auto const& group : groups)
        for (auto const& drawable : group.drawables.get())
          checked(painter.draw_drawable_projected(drawable, drawable.position, 1, 1));
    auto image = pixels();
    Pixel const red{255, 0, 0, 255};
    expectPixel(image, Width, 5, 5, red);
    expectPixel(image, Width, 15, 15, vehicle_layer > 20u ? red : Pixel{0, 255, 0, 255});
    expectPixel(image, Width, 45, 32, vehicle_layer > 20u ? red : Pixel{0, 0, 255, 255});
    checked(renderer->finishFrame());
  }
}

TEST_F(OpenGlRendererRegression, ProjectedDrawableEdgesAndVertices) {
  DrawablePainter painter(renderer, {});
  RectF const viewport(0, 0, Width, Height);
  auto const red = Color::rgb(255, 0, 0);
  auto const green = Color::rgb(0, 255, 0);
  auto const blue = Color::rgb(0, 0, 255);
  auto const yellow = Color::rgb(255, 255, 0);
  checked(renderer->loadConfig(json(R"({})")));
  solid("geometry", R"({"includeVBTextures":false})");

  for (float zoom : {1.0f, 2.0f}) {
    SCOPED_TRACE(testing::Message() << "zoom " << zoom);
    checked(renderer->startFrame());
    select("geometry");
    quad(Width, Height, {0, 0, 0, 255});
    checked(renderer->flush());
    auto& submitted = renderer->immediatePrimitives();
    ASSERT_TRUE(submitted.empty());
    auto stroke = [&](Vec2F start, Vec2F end, Color const& color, bool fullbright,
                      std::optional<Color> tail = {}, float width = 8.0f) {
      float scale = tile_pixels * zoom;
      Drawable::LinePart part{Line2F({}, (end - start) / scale), width, tail};
      painter.draw_projected_line(part, color, fullbright, start, scale, zoom, std::cref(viewport));
    };
    // All three centerlines miss the viewport; the projected strokes do not.
    stroke({12, -1}, {72, -1}, red, false, blue);
    stroke({Width + 1, 12}, {Width + 1, 52}, green, true);
    stroke({30, Height + 1}, {70, Height + 11}, blue, false, {}, -8.0f);
    stroke({12, -1 - 4 * zoom}, {72, -1 - 4 * zoom}, red, true);
    stroke({-10, 20}, {-1, 20}, red, true); // Broad bound overlaps, but the butt cap misses.
    stroke({-400, 20}, {-390, 20}, red, true); // Fully outside even the broad bound.
    ASSERT_EQ(submitted.size(), 3u);
    auto const& gradient = std::get<RenderQuad>(submitted[0]);
    auto expect_corner = [](RenderVertex const& vertex, float x, float y) {
      EXPECT_FLOAT_EQ(vertex.screenCoordinate.x(), x);
      EXPECT_FLOAT_EQ(vertex.screenCoordinate.y(), y);
    };
    expect_corner(gradient.a, 12, -1 + 4 * zoom);
    expect_corner(gradient.b, 12, -1 - 4 * zoom);
    expect_corner(gradient.c, 72, -1 - 4 * zoom);
    expect_corner(gradient.d, 72, -1 + 4 * zoom);
    EXPECT_EQ(gradient.a.color, red.to_rgba());
    EXPECT_EQ(gradient.b.color, red.to_rgba());
    EXPECT_EQ(gradient.c.color, blue.to_rgba());
    EXPECT_EQ(gradient.d.color, blue.to_rgba());
    EXPECT_EQ(gradient.a.param1, 1.0f);
    EXPECT_EQ(std::get<RenderQuad>(submitted[1]).a.param1, 0.0f);

    auto polygon = Drawable::make_poly(PolyF{{-2, -2}, {3, -1}, {2, 2}, {-3, 1}}, yellow);
    polygon.fullbright = false; // Polygons retain their historical fixed zero light parameter.
    checked(painter.draw_drawable_projected(polygon, {48, 34}, 4, zoom));
    ASSERT_EQ(submitted.size(), 4u);
    auto const& poly = std::get<RenderPoly>(submitted[3]);
    EXPECT_FALSE(poly.texture);
    std::array<Vec2F, 4> const expected{{{40, 26}, {60, 30}, {56, 42}, {36, 38}}};
    ASSERT_EQ(poly.vertexes.size(), expected.size());
    for (std::size_t i = 0; i != expected.size(); ++i) {
      EXPECT_EQ(poly.vertexes[i].screenCoordinate, expected[i]);
      EXPECT_EQ(poly.vertexes[i].textureCoordinate, Vec2F());
      EXPECT_EQ(poly.vertexes[i].color, yellow.to_rgba());
      EXPECT_EQ(poly.vertexes[i].param1, 0.0f);
    }
    auto image = pixels();
    expectPixel(image, Width, 42, 0, {125, 0, 130, 255});
    expectPixel(image, Width, 42, static_cast<unsigned>(4 * zoom - 1), {0, 0, 0, 255});
    expectPixel(image, Width, Width - 1, 30, {0, 255, 0, 255});
    expectPixel(image, Width, 32, Height - 1, {0, 0, 255, 255});
    expectPixel(image, Width, 48, 34, {255, 255, 0, 255});
    checked(renderer->finishFrame());
  }
}

TEST_F(OpenGlRendererRegression, UnsupportedOutputKeepsLastGoodModeAndFrame) {
  checked(renderer->loadConfig(json(R"({})")));
  solid("window", R"({"includeVBTextures":false})");
  RenderPreferences prefs;
  RenderOutputSettings previous{RenderOutputMode::Legacy, 1, 250, 900};
  checked(prefs.applyOutput(*renderer, previous));
  auto saved = json(R"({"renderOutput":"hdr10","renderExposure":2,"hdrPaperWhite":250,"hdrPeakLuminance":900,"vsync":true,"renderFpsLimit":0})");
  checked(prefs.observe(saved));
  auto rejected = prefs.applyOutput(*renderer, prefs.output);
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<OutputPreferenceRejected>());
  EXPECT_FALSE(prefs.outputNotice.empty());
  checked(prefs.observe(saved));
  EXPECT_EQ(prefs.output, previous);
  checked(prefs.applyOutput(*renderer, prefs.output));
  checked(renderer->startFrame());
  select("window");
  pattern(Width, Height);
  expectPattern(pixels());
  checked(renderer->finishFrame());
}

TEST_F(VulkanRendererRegression, OutputChangeInsideFrameIsNotAPreferenceRejection) {
  RenderPreferences prefs;
  checked(renderer->startFrame());
  auto rejected = prefs.applyOutput(*renderer, {RenderOutputMode::Sdr});
  ASSERT_FALSE(rejected);
  EXPECT_FALSE(rejected.error().is<OutputPreferenceRejected>());
  EXPECT_TRUE(prefs.outputNotice.empty());
  quad(Width, Height, {255, 0, 0, 255});
  expectPixel(pixels(), Width, 2, 2, {255, 0, 0, 255});
}

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

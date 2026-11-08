#include <stddef.h>
#include "GL/glew.h"
import std;
import star.memory;
import star.format;
import star.json;
import star.string;
import star.list;
import star.json_extra;
import star.rect;
import star.image;
import star.casting;
import star.texture_atlas;
import star.bi_map;
import star.ref_ptr;
import star.renderer;

import star.renderer_opengl;
import star.logging;

import star.vector;
import star.matrix3;
import star.math_common;
import star.algorithm;
namespace Star {

std::size_t const MultiTextureCount = 4;

namespace {
struct GlTextureBindingGuard {
  GLint texture = 0;
  GLint multisampleTexture = 0;
  GLint unpackAlignment = 0;
  GlTextureBindingGuard() {
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    if (GLEW_VERSION_3_2)
      glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &multisampleTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
  }
  ~GlTextureBindingGuard() {
    glBindTexture(GL_TEXTURE_2D, texture);
    if (GLEW_VERSION_3_2)
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, multisampleTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
  }
};
struct GlFramebufferBindingGuard {
  GLint read = 0;
  GLint draw = 0;
  GlFramebufferBindingGuard() {
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
  }
  ~GlFramebufferBindingGuard() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
  }
};

void uploadEffectParameter(GLint uniform, RenderEffectParameter const& value) {
  std::visit([&](auto const& v) {
    using T = std::remove_cvref_t<decltype(v)>;
    if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int>)
      glUniform1i(uniform, v);
    else if constexpr (std::is_same_v<T, float>)
      glUniform1f(uniform, v);
    else if constexpr (std::is_same_v<T, Vec2F>)
      glUniform2f(uniform, v[0], v[1]);
    else if constexpr (std::is_same_v<T, Vec3F>)
      glUniform3f(uniform, v[0], v[1], v[2]);
    else if constexpr (std::is_same_v<T, Vec4F>)
      glUniform4f(uniform, v[0], v[1], v[2], v[3]);
  }, value);
}
}

constexpr char DefaultVertexShader[] = R"SHADER(
#version 140

uniform vec2 textureSize0;
uniform vec2 textureSize1;
uniform vec2 textureSize2;
uniform vec2 textureSize3;
uniform vec2 screenSize;
uniform mat3 vertexTransform;

in vec2 vertexPosition;
in vec4 vertexColor;
in vec2 vertexTextureCoordinate;
in int vertexData;

out vec2 fragmentTextureCoordinate;
flat out int fragmentTextureIndex;
out vec4 fragmentColor;

void main() {
  vec2 screenPosition = (vertexTransform * vec3(vertexPosition, 1.0)).xy;
  gl_Position = vec4(screenPosition / screenSize * 2.0 - 1.0, 0.0, 1.0);
  if (((vertexData >> 3) & 0x1) == 1)
    screenPosition.x = round(screenPosition.x);
  if (((vertexData >> 4) & 0x1) == 1)
    screenPosition.y = round(screenPosition.y);
  int vertexTextureIndex = vertexData & 0x3;
  if (vertexTextureIndex == 3)
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize3;
  else if (vertexTextureIndex == 2)
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize2;
  else if (vertexTextureIndex == 1)
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize1;
  else
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize0;

  fragmentTextureIndex = vertexTextureIndex;
  fragmentColor = vertexColor;
}
)SHADER";

constexpr char DefaultFragmentShader[] = R"SHADER(
#version 140

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;

in vec2 fragmentTextureCoordinate;
flat in int fragmentTextureIndex;
in vec4 fragmentColor;

out vec4 outColor;

void main() {
  vec4 texColor;
  if (fragmentTextureIndex == 3)
    texColor = texture(texture3, fragmentTextureCoordinate);
  else if (fragmentTextureIndex == 2)
    texColor = texture(texture2, fragmentTextureCoordinate);
  else if (fragmentTextureIndex == 1)
    texColor = texture(texture1, fragmentTextureCoordinate);
  else
    texColor = texture(texture0, fragmentTextureCoordinate);

  if (texColor.a <= 0.0)
    discard;

  outColor = texColor * fragmentColor;
}
)SHADER";



bool settingModeValue(BoolSettingMode const& mode, bool const& setting) {
  if (mode == BoolSettingMode::FromSetting) {
    return setting;
  } else {
    return mode == BoolSettingMode::Enabled;
  }
}

OpenGlRenderer::OpenGlRenderer(Logger& logger)
  : m_logger(logger), m_limitTextureGroupSize(false), m_useMultiTexturing(true),
    m_multiSampling(0), m_hdrSetting(true) {}

std::expected<std::shared_ptr<OpenGlRenderer>, StarException> OpenGlRenderer::create(Logger& logger) {
  auto renderer = std::shared_ptr<OpenGlRenderer>(new OpenGlRenderer(logger));
  auto initialized = renderer->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return renderer;
}

std::expected<void, StarException> OpenGlRenderer::initialize() {
  auto glewResult = glewInit();
  if (glewResult != GLEW_OK && glewResult != GLEW_ERROR_NO_GLX_DISPLAY)
    return std::unexpected(RendererException::format("Could not initialize GLEW: {}", (char*)glewGetErrorString(glewResult)));
  // GLEW probes an extension with glGetString on core profiles.
  logGlErrorSummary("OpenGL errors during GLEW initialization");

  auto version = reinterpret_cast<char const*>(glGetString(GL_VERSION));
  auto vendor = reinterpret_cast<char const*>(glGetString(GL_VENDOR));
  auto renderer = reinterpret_cast<char const*>(glGetString(GL_RENDERER));
  auto shaderVersion = reinterpret_cast<char const*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
  auto queried = checkGlError("OpenGL version query");
  if (!queried) return std::unexpected(std::move(queried.error()));
  if (!version || !vendor || !renderer || !shaderVersion)
    return std::unexpected(RendererException("OpenGL returned a null version or renderer string"));
  auto logged = m_logger.info("OpenGL version: '{}' vendor: '{}' renderer: '{}' shader: '{}'",
      version, vendor, renderer, shaderVersion);
  if (!logged) return std::unexpected(std::move(logged.error()));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);

  m_limitTextureGroupSize = false;
  m_useMultiTexturing = true;
  m_multiSampling = false;
  m_hdrSetting = true;

  auto whiteTexture = createGlTexture(Image::filled({1, 1}, Vec4B(255, 255, 255, 255), PixelFormat::RGBA32),
      TextureAddressing::Clamp, TextureFiltering::Nearest);
  if (!whiteTexture)
    return std::unexpected(std::move(whiteTexture.error()));
  m_whiteTexture = std::move(*whiteTexture);
  auto buffer = createGlRenderBuffer();
  if (!buffer)
    return std::unexpected(std::move(buffer.error()));
  m_immediateRenderBuffer = std::move(*buffer);
  auto effect = loadEffectConfig("internal", JsonObject(), {{"vertex", DefaultVertexShader}, {"fragment", DefaultFragmentShader}});
  if (!effect)
    return std::unexpected(std::move(effect.error()));
  return checkGlError("renderer initialization");
}

OpenGlRenderer::~OpenGlRenderer() {
  for (auto&& effect : m_effects)
    glDeleteProgram(effect.second.program);

  m_frameBuffers.clear();
  logGlErrorSummary("OpenGL errors during shutdown");
}

String OpenGlRenderer::rendererId() const {
  return "OpenGL20";
}

Vec2U OpenGlRenderer::screenSize() const {
  return m_screenSize;
}

std::expected<RefPtr<OpenGlRenderer::GlFrameBuffer>, StarException> OpenGlRenderer::GlFrameBuffer::create(Json const& fbConfig) {
  auto result = make_ref<GlFrameBuffer>();
  result->config = fbConfig;
  auto clear = fbConfig.getBool("clear", true);
  if (!clear) return std::unexpected(std::move(clear.error()));
  result->clear = *clear;
  auto multisample = fbConfig.getUInt("multisample", 0);
  if (!multisample) return std::unexpected(std::move(multisample.error()));
  if (*multisample > static_cast<std::uint64_t>(std::numeric_limits<GLsizei>::max()))
    return std::unexpected(RendererException("Framebuffer sample count exceeds the OpenGL integer range"));
  result->multisample = GLEW_VERSION_4_0 ? *multisample : 0;
  auto hdrName = fbConfig.getString("hdr", "Disabled");
  if (!hdrName) return std::unexpected(std::move(hdrName.error()));
  auto hdrMode = enumValue<BoolSettingModeNames>(*hdrName);
  if (!hdrMode) return std::unexpected(std::move(hdrMode.error()));
  result->hdrMode = *hdrMode;
  auto hdrSetting = fbConfig.getBool("hdrSetting", false);
  if (!hdrSetting) return std::unexpected(std::move(hdrSetting.error()));
  bool hdr = settingModeValue(result->hdrMode, *hdrSetting);
  auto alpha = fbConfig.getBool("alpha", false);
  if (!alpha) return std::unexpected(std::move(alpha.error()));
  result->alpha = *alpha || result->multisample;
  auto sizeDiv = fbConfig.getUInt("sizeDiv", 1);
  if (!sizeDiv) return std::unexpected(std::move(sizeDiv.error()));
  if (*sizeDiv == 0 || *sizeDiv > std::numeric_limits<unsigned>::max())
    return std::unexpected(RendererException("Framebuffer sizeDiv must be a positive unsigned integer"));
  result->sizeDiv = *sizeDiv;
  Vec2U size(256, 256);
  auto configuredSize = fbConfig.optArray("size");
  if (!configuredSize) return std::unexpected(std::move(configuredSize.error()));
  if (*configuredSize) {
    auto parsedSize = jsonToVec2U(**configuredSize);
    if (!parsedSize) return std::unexpected(std::move(parsedSize.error()));
    result->overrideSize = *parsedSize;
    size = *parsedSize;
  }
  if (!size[0] || !size[1])
    return std::unexpected(RendererException("Framebuffer size must be positive"));
  auto addressingName = fbConfig.getString("textureAddressing", "clamp");
  if (!addressingName) return std::unexpected(std::move(addressingName.error()));
  auto addressing = enumValue<TextureAddressingNames>(*addressingName);
  if (!addressing) return std::unexpected(std::move(addressing.error()));
  auto filteringName = fbConfig.getString("textureFiltering", "nearest");
  if (!filteringName) return std::unexpected(std::move(filteringName.error()));
  auto filtering = enumValue<TextureFilteringNames>(*filteringName);
  if (!filtering) return std::unexpected(std::move(filtering.error()));

  GlTextureBindingGuard textureBinding;
  GlFramebufferBindingGuard framebufferBinding;
  result->texture = make_ref<GlLoneTexture>();
  auto& texture = result->texture;
  texture->textureFiltering = *filtering;
  texture->textureAddressing = *addressing;
  texture->textureSize = size;
  glGenTextures(1, &texture->textureId);
  auto textureStatus = checkGlError("framebuffer texture creation");
  if (!textureStatus) return std::unexpected(std::move(textureStatus.error()));
  if (!texture->textureId)
    return std::unexpected(RendererException("Could not generate OpenGL texture for framebuffer"));
  GLenum target = result->multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  glBindTexture(target, texture->textureId);
  if (result->multisample) {
    glTexImage2DMultisample(target, result->multisample, hdr ? GL_RGBA16F : GL_RGBA8, size[0], size[1], GL_TRUE);
  } else {
    GLenum format = result->alpha ? GL_RGBA : GL_RGB;
    GLenum internalFormat = hdr ? (result->alpha ? GL_RGBA16F : GL_RGB16F) : (result->alpha ? GL_RGBA8 : GL_RGB8);
    glTexImage2D(target, 0, internalFormat, size[0], size[1], 0, format, hdr ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    GLint wrap = *addressing == TextureAddressing::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    GLint filter = *filtering == TextureFiltering::Nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
  }
  glGenFramebuffers(1, &result->id);
  auto framebufferStatus = checkGlError("framebuffer handle creation");
  if (!framebufferStatus) return std::unexpected(std::move(framebufferStatus.error()));
  if (!result->id)
    return std::unexpected(RendererException("Failed to create OpenGL framebuffer"));
  glBindFramebuffer(GL_FRAMEBUFFER, result->id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, texture->textureId, 0);
  auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
    return std::unexpected(RendererException::format("OpenGL framebuffer is not complete: {}", status));
  auto glStatus = checkGlError("framebuffer creation");
  if (!glStatus) return std::unexpected(std::move(glStatus.error()));
  return result;
}

std::expected<void, StarException> OpenGlRenderer::GlFrameBuffer::makeAlt(Vec2U const& screenSize) {
  auto size = overrideSize ? *overrideSize : screenSize / sizeDiv;
  auto alternateConfig = config.set("size", JsonArray{size[0], size[1]});
  if (!alternateConfig) return std::unexpected(std::move(alternateConfig.error()));
  auto alternate = create(*alternateConfig);
  if (!alternate) return std::unexpected(std::move(alternate.error()));
  glDeleteFramebuffers(1, &altId);
  altId = std::exchange((*alternate)->id, 0);
  altTexture = std::move((*alternate)->texture);
  hasAlt = true;
  return {};
}

std::expected<void, StarException> OpenGlRenderer::GlFrameBuffer::swap() {
  if (!hasAlt)
    return std::unexpected(RendererException("Attempting to swap framebuffer without an alt!"));
  
  std::swap(id,altId);
  std::swap(texture,altTexture);
  justSwapped = true;
  return {};
}

OpenGlRenderer::GlFrameBuffer::~GlFrameBuffer() {
  glDeleteFramebuffers(1, &id);
  glDeleteFramebuffers(1, &altId);
  altTexture.reset();
  texture.reset();
}

std::expected<void, StarException> OpenGlRenderer::loadConfig(Json const& config) {
  auto definitions = config.getObject("frameBuffers", {});
  if (!definitions) return std::unexpected(std::move(definitions.error()));
  StringMap<RefPtr<GlFrameBuffer>> buffers;
  for (auto const& pair : *definitions) {
    auto multisampleConfig = pair.second.set("multisample", m_multiSampling);
    if (!multisampleConfig) return std::unexpected(std::move(multisampleConfig.error()));
    auto framebufferConfig = multisampleConfig->set("hdrSetting", m_hdrSetting);
    if (!framebufferConfig) return std::unexpected(std::move(framebufferConfig.error()));
    auto doubleBuffered = framebufferConfig->getBool("double", false);
    if (!doubleBuffered) return std::unexpected(std::move(doubleBuffered.error()));
    auto logged = m_logger.info("Creating framebuffer {}", pair.first);
    if (!logged) return std::unexpected(std::move(logged.error()));
    auto buffer = GlFrameBuffer::create(*framebufferConfig);
    if (!buffer) return std::unexpected(std::move(buffer.error()));
    if (*doubleBuffered) {
      auto alternate = (*buffer)->makeAlt(m_screenSize);
      if (!alternate) return std::unexpected(std::move(alternate.error()));
    }
    buffers[pair.first] = std::move(*buffer);
  }
  auto previous = std::move(m_frameBuffers);
  m_frameBuffers = std::move(buffers);
  auto resized = setScreenSize(m_screenSize);
  if (!resized) {
    m_frameBuffers = std::move(previous);
    return std::unexpected(std::move(resized.error()));
  }
  m_currentFrameBuffer.reset();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  for (auto&& effect : m_effects)
    for (auto&& texture : effect.second.textures)
      texture.second.textureValue.reset();
  m_config = config;
  return {};
}

std::expected<void, StarException> OpenGlRenderer::loadEffectConfig(String const& name, Json const& effectConfig, StringMap<String> const& shaders) {
  auto compileShader = [](GLenum type, std::optional<std::string_view> source, std::string_view name) -> std::expected<GLuint, StarException> {
    if (!source)
      return GLuint{0};
    GLuint shader = glCreateShader(type);
    auto created = checkGlError("shader creation");
    if (!created) {
      if (shader) glDeleteShader(shader);
      return std::unexpected(std::move(created.error()));
    }
    if (!shader)
      return std::unexpected(RendererException::format("Could not create {} shader", name));
    auto sourceData = source->data();
    GLint sourceLength = source->size();
    glShaderSource(shader, 1, &sourceData, &sourceLength);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
      char log[1024]{};
      glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
      glDeleteShader(shader);
      return std::unexpected(RendererException::format("Failed to compile {} shader: {}", name, log));
    }
    return shader;
  };
  auto compilePair = [&](std::optional<std::string_view> vertexSource, std::optional<std::string_view> fragmentSource) -> std::expected<std::pair<GLuint, GLuint>, StarException> {
    auto vertex = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex");
    if (!vertex) return std::unexpected(std::move(vertex.error()));
    auto fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    if (!fragment) {
      glDeleteShader(*vertex);
      return std::unexpected(std::move(fragment.error()));
    }
    return std::pair{*vertex, *fragment};
  };
  auto vertexSource = shaders.find_ref("vertex");
  auto fragmentSource = shaders.find_ref("fragment");
  auto compiled = compilePair(
      vertexSource ? std::optional<std::string_view>{vertexSource->get().utf8()} : std::nullopt,
      fragmentSource ? std::optional<std::string_view>{fragmentSource->get().utf8()} : std::nullopt);
  if (!compiled) {
    auto logged = m_logger.error("Shader compile error, using default: {}", compiled.error().what());
    if (!logged) return std::unexpected(std::move(logged.error()));
    compiled = compilePair(DefaultVertexShader, DefaultFragmentShader);
    if (!compiled) return std::unexpected(std::move(compiled.error()));
  }
  GLuint program = glCreateProgram();
  if (program) {
    if (compiled->first) glAttachShader(program, compiled->first);
    if (compiled->second) glAttachShader(program, compiled->second);
    glLinkProgram(program);
  }
  glDeleteShader(compiled->first);
  glDeleteShader(compiled->second);
  auto linked = checkGlError("shader program creation and linking");
  if (!linked) {
    if (program) glDeleteProgram(program);
    return std::unexpected(std::move(linked.error()));
  }
  if (!program)
    return std::unexpected(RendererException("Could not create OpenGL program"));
  struct ProgramGuard {
    GLuint program;
    ~ProgramGuard() { if (program) glDeleteProgram(program); }
  } programGuard{program};
  GLint status = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (!status) {
    char log[1024]{};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    return std::unexpected(RendererException::format("Failed to link program: {}", log));
  }

  Effect effect;
  effect.program = program;
  effect.config = effectConfig;
  auto includeTextures = effectConfig.getBool("includeVBTextures", true);
  if (!includeTextures) return std::unexpected(std::move(includeTextures.error()));
  effect.includeVBTextures = *includeTextures;
  auto parameters = effectConfig.getObject("effectParameters", {});
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  for (auto const& entry : *parameters) {
    auto uniform = entry.second.getString("uniform");
    if (!uniform) return std::unexpected(std::move(uniform.error()));
    EffectParameter parameter;
    parameter.parameterUniform = glGetUniformLocation(program, uniform->utf8_data());
    if (parameter.parameterUniform == -1) {
      auto logged = m_logger.warn("OpenGL20 effect parameter '{}' in effect '{}' has no associated uniform, skipping", entry.first, name);
      if (!logged) return std::unexpected(std::move(logged.error()));
      continue;
    }
    auto type = entry.second.getString("type");
    if (!type) return std::unexpected(std::move(type.error()));
    if (*type == "bool") parameter.parameterType = 5;
    else if (*type == "int") parameter.parameterType = 1;
    else if (*type == "float") parameter.parameterType = 0;
    else if (*type == "vec2") parameter.parameterType = 4;
    else if (*type == "vec3") parameter.parameterType = 3;
    else if (*type == "vec4") parameter.parameterType = 2;
    else return std::unexpected(RendererException::format("Unrecognized effect parameter type '{}'", *type));
    auto scriptable = entry.second.getBool("scriptable", false);
    if (!scriptable) return std::unexpected(std::move(scriptable.error()));
    auto value = entry.second.get("default", {});
    if (!value) return std::unexpected(std::move(value.error()));
    if (!value->isNull()) {
      auto parseValue = [&]() -> std::expected<RenderEffectParameter, StarException> {
        if (*type == "bool") {
          auto parsed = value->toBool();
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          return RenderEffectParameter(*parsed);
        }
        if (*type == "int") {
          auto parsed = value->toInt();
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          return RenderEffectParameter(static_cast<int>(*parsed));
        }
        if (*type == "float") {
          auto parsed = value->toFloat();
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          return RenderEffectParameter(*parsed);
        }
        if (*type == "vec2") {
          auto parsed = jsonToVec2F(*value);
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          return RenderEffectParameter(*parsed);
        }
        if (*type == "vec3") {
          auto parsed = jsonToVec3F(*value);
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          return RenderEffectParameter(*parsed);
        }
        auto parsed = jsonToVec4F(*value);
        if (!parsed) return std::unexpected(std::move(parsed.error()));
        return RenderEffectParameter(*parsed);
      };
      auto parsed = parseValue();
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      parameter.parameterValue = std::move(*parsed);
    }
    if (*scriptable) effect.scriptables[entry.first] = std::move(parameter);
    else effect.parameters[entry.first] = std::move(parameter);
  }
  unsigned textureUnit = effect.includeVBTextures ? MultiTextureCount : 0;
  auto textures = effectConfig.getObject("effectTextures", {});
  if (!textures) return std::unexpected(std::move(textures.error()));
  for (auto const& entry : *textures) {
    auto uniform = entry.second.getString("textureUniform");
    if (!uniform) return std::unexpected(std::move(uniform.error()));
    EffectTexture texture;
    texture.textureUniform = glGetUniformLocation(program, uniform->utf8_data());
    if (texture.textureUniform == -1) {
      auto logged = m_logger.warn("OpenGL20 effect parameter '{}' has no associated uniform, skipping", entry.first);
      if (!logged) return std::unexpected(std::move(logged.error()));
      continue;
    }
    texture.textureUnit = textureUnit++;
    auto addressingName = entry.second.getString("textureAddressing", "clamp");
    if (!addressingName) return std::unexpected(std::move(addressingName.error()));
    auto addressing = enumValue<TextureAddressingNames>(*addressingName);
    if (!addressing) return std::unexpected(std::move(addressing.error()));
    texture.textureAddressing = *addressing;
    auto filteringName = entry.second.getString("textureFiltering", "nearest");
    if (!filteringName) return std::unexpected(std::move(filteringName.error()));
    auto filtering = enumValue<TextureFilteringNames>(*filteringName);
    if (!filtering) return std::unexpected(std::move(filtering.error()));
    texture.textureFiltering = *filtering;
    auto sizeUniform = entry.second.optString("textureSizeUniform");
    if (!sizeUniform) return std::unexpected(std::move(sizeUniform.error()));
    if (*sizeUniform) {
      texture.textureSizeUniform = glGetUniformLocation(program, (**sizeUniform).utf8_data());
      if (texture.textureSizeUniform == -1) {
        auto logged = m_logger.warn("OpenGL20 effect parameter '{}' has textureSizeUniform '{}' with no associated uniform", entry.first, **sizeUniform);
        if (!logged) return std::unexpected(std::move(logged.error()));
      }
    }
    effect.textures[entry.first] = std::move(texture);
  }
  auto output = effectConfig.optString("frameBuffer");
  if (!output) return std::unexpected(std::move(output.error()));
  auto blit = effectConfig.optString("blitFrameBuffer");
  if (!blit) return std::unexpected(std::move(blit.error()));
  auto framebufferTextures = effectConfig.optArray("frameBufferTextures");
  if (!framebufferTextures) return std::unexpected(std::move(framebufferTextures.error()));
  if (*output) {
    effect.doubleBuffered = *blit && **output == **blit;
    if (!effect.doubleBuffered && *framebufferTextures) {
      for (auto const& texture : **framebufferTextures) {
        auto input = texture.optString("framebuffer");
        if (!input) return std::unexpected(std::move(input.error()));
        if (*input && **input == **output) {
          effect.doubleBuffered = true;
          break;
        }
      }
    }
  }
  auto flushed = flushImmediatePrimitives();
  if (!flushed) return std::unexpected(std::move(flushed.error()));
  glUseProgram(program);
  setupGlUniforms(effect, m_screenSize);
  auto glStatus = checkGlError("effect configuration");
  if (!glStatus) {
    glUseProgram(m_program);
    if (m_currentEffect) {
      auto size = m_currentFrameBuffer
          ? (m_currentFrameBuffer->overrideSize ? *m_currentFrameBuffer->overrideSize : m_screenSize / m_currentFrameBuffer->sizeDiv)
          : m_screenSize;
      setupGlUniforms(m_currentEffect->get(), size);
    }
    return std::unexpected(std::move(glStatus.error()));
  }
  if (auto old = m_effects.find_ref(name))
    glDeleteProgram(old->get().program);
  m_effects[name] = std::move(effect);
  m_currentEffect = std::ref(m_effects[name]);
  m_program = program;
  programGuard.program = 0;
  return {};
}

std::expected<void, StarException> OpenGlRenderer::setEffectParameter(String const& parameterName, RenderEffectParameter const& value) {
  auto parameter = m_currentEffect->get().parameters.find_ref(parameterName);
  if (!parameter || (parameter->get().parameterValue && *parameter->get().parameterValue == value))
    return {};

  if (parameter->get().parameterType != static_cast<std::uint8_t>(value.index()))
    return std::unexpected(RendererException::format("OpenGlRenderer::setEffectParameter '{}' parameter type mismatch", parameterName));

  auto flushed = flushImmediatePrimitives();
  if (!flushed) return std::unexpected(std::move(flushed.error()));

  uploadEffectParameter(parameter->get().parameterUniform, value);

  auto status = checkGlError("effect parameter update");
  if (!status) return std::unexpected(std::move(status.error()));
  parameter->get().parameterValue = value;
  return {};
}

std::expected<void, StarException> OpenGlRenderer::setEffectScriptableParameter(String const& effectName, String const& parameterName, RenderEffectParameter const& value) {
  auto find = m_effects.find(effectName);
  if (find == m_effects.end())
    return {};

  Effect& effect = find->second;

  auto parameter = effect.scriptables.find_ref(parameterName);
  if (!parameter)
    return {};

  // Lua numbers arrive as floats even for declared integer parameters. Keep
  // this coercion beside validation so every caller gets the same semantics.
  if (parameter->get().parameterType == 1 && std::holds_alternative<float>(value)) {
    parameter->get().parameterValue = static_cast<int>(std::get<float>(value));
    return {};
  }
  if (parameter->get().parameterType != static_cast<std::uint8_t>(value.index()))
    return std::unexpected(RendererException::format("OpenGlRenderer::setEffectScriptableParameter '{}' parameter type mismatch", parameterName));

  parameter->get().parameterValue = value;
  return {};
}

std::optional<RenderEffectParameter> OpenGlRenderer::getEffectScriptableParameter(String const& effectName, String const& parameterName) {
  auto find = m_effects.find(effectName);
  if (find == m_effects.end())
    return {};

  Effect& effect = find->second;

  auto parameter = effect.scriptables.find_ref(parameterName);
  if (!parameter)
    return {};
  
  return parameter->get().parameterValue;
}

std::expected<void, StarException> OpenGlRenderer::setEffectTexture(String const& textureName, ImageView const& image) {
  auto textureParameter = m_currentEffect->get().textures.find_ref(textureName);
  if (!textureParameter)
    return {};
  auto& parameter = textureParameter->get();
  auto flushed = flushImmediatePrimitives();
  if (!flushed) return std::unexpected(std::move(flushed.error()));
  GlTextureBindingGuard binding;
  if (!parameter.textureValue || parameter.textureValue->textureId == 0) {
    auto texture = createGlTexture(image, parameter.textureAddressing, parameter.textureFiltering);
    if (!texture) return std::unexpected(std::move(texture.error()));
    parameter.textureValue = std::move(*texture);
  } else {
    glBindTexture(GL_TEXTURE_2D, parameter.textureValue->textureId);
    auto uploaded = uploadTextureImage(image.format, image.size, image.data.data());
    if (!uploaded) return std::unexpected(std::move(uploaded.error()));
    parameter.textureValue->textureSize = image.size;
  }
  if (parameter.textureSizeUniform != -1) {
    auto textureSize = parameter.textureValue->glTextureSize();
    glUniform2f(parameter.textureSizeUniform, textureSize[0], textureSize[1]);
  }
  return checkGlError("effect texture update");
}

std::expected<bool, StarException> OpenGlRenderer::switchEffectConfig(String const& name) {
  auto flushed = flushImmediatePrimitives();
  if (!flushed) return std::unexpected(std::move(flushed.error()));
  auto find = m_effects.find(name);
  if (find == m_effects.end())
    return false;
  Effect& effect = find->second;
  if (m_currentEffect && std::addressof(m_currentEffect->get()) == std::addressof(effect))
    return true;
  auto output = effect.config.optString("frameBuffer");
  if (!output) return std::unexpected(std::move(output.error()));
  auto inputs = effect.config.optArray("frameBufferTextures");
  if (!inputs) return std::unexpected(std::move(inputs.error()));
  auto blit = effect.config.optString("blitFrameBuffer");
  if (!blit) return std::unexpected(std::move(blit.error()));
  RefPtr<GlFrameBuffer> outputBuffer;
  RefPtr<GlFrameBuffer> blitBuffer;
  List<std::pair<std::reference_wrapper<EffectTexture>, RefPtr<GlFrameBuffer>>> inputBuffers;
  if (*output) {
    auto buffer = getGlFrameBuffer(**output);
    if (!buffer) return std::unexpected(std::move(buffer.error()));
    outputBuffer = *buffer;
    if (effect.doubleBuffered && !outputBuffer->hasAlt) {
      auto logged = m_logger.warn("Effect {} should be double buffered, but framebuffer {} doesn't have an alt! Making one!", name, **output);
      if (!logged) return std::unexpected(std::move(logged.error()));
      auto alternate = outputBuffer->makeAlt(m_screenSize);
      if (!alternate) return std::unexpected(std::move(alternate.error()));
    }
  }
  if (*inputs) {
    for (auto const& input : **inputs) {
      auto framebuffer = input.optString("framebuffer");
      if (!framebuffer) return std::unexpected(std::move(framebuffer.error()));
      if (!*framebuffer) continue;
      auto textureName = input.getString("texture");
      if (!textureName) return std::unexpected(std::move(textureName.error()));
      if (auto texture = effect.textures.find_ref(*textureName)) {
        auto buffer = getGlFrameBuffer(**framebuffer);
        if (!buffer) return std::unexpected(std::move(buffer.error()));
        inputBuffers.emplace_back(*texture, *buffer);
      }
    }
  }
  if (*blit) {
    auto buffer = getGlFrameBuffer(**blit);
    if (!buffer) return std::unexpected(std::move(buffer.error()));
    blitBuffer = *buffer;
  }
  struct SwitchGuard {
    OpenGlRenderer& renderer;
    std::optional<std::reference_wrapper<Effect>> effect;
    GLuint program;
    RefPtr<GlFrameBuffer> framebuffer;
    RefPtr<GlFrameBuffer> swapped;
    GLint draw = 0;
    GLint read = 0;
    bool committed = false;
    ~SwitchGuard() {
      if (committed) return;
      if (swapped) {
        std::swap(swapped->id, swapped->altId);
        std::swap(swapped->texture, swapped->altTexture);
        swapped->justSwapped = false;
      }
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
      renderer.m_currentFrameBuffer = framebuffer;
      renderer.m_currentEffect = effect;
      renderer.m_program = program;
      glUseProgram(program);
      if (effect) {
        auto size = framebuffer
            ? (framebuffer->overrideSize ? *framebuffer->overrideSize : renderer.m_screenSize / framebuffer->sizeDiv)
            : renderer.m_screenSize;
        renderer.setupGlUniforms(effect->get(), size);
      }
    }
  } guard{*this, m_currentEffect, m_program, m_currentFrameBuffer, {}};
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &guard.draw);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &guard.read);
  auto effectSize = m_screenSize;
  if (outputBuffer) {
    effectSize = outputBuffer->overrideSize ? *outputBuffer->overrideSize : m_screenSize / outputBuffer->sizeDiv;
    if (effect.doubleBuffered) {
      auto swapped = outputBuffer->swap();
      if (!swapped) return std::unexpected(std::move(swapped.error()));
      guard.swapped = outputBuffer;
    }
    auto switched = switchGlFrameBuffer(outputBuffer);
    if (!switched) return std::unexpected(std::move(switched.error()));
  } else {
    m_currentFrameBuffer.reset();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  }
  glUseProgram(m_program = effect.program);
  setupGlUniforms(effect, effectSize);
  m_currentEffect = std::ref(effect);
  auto rounding = setEffectParameter("vertexRounding", m_multiSampling > 0);
  if (!rounding) return std::unexpected(std::move(rounding.error()));
  for (auto const& [parameterRef, buffer] : inputBuffers) {
    auto& parameter = parameterRef.get();
    bool undefined = !parameter.textureValue || parameter.textureValue->textureId == 0;
    if (undefined || buffer->hasAlt) {
      parameter.textureValue = effect.doubleBuffered && buffer == outputBuffer ? buffer->altTexture : buffer->texture;
      if (parameter.textureSizeUniform != -1 && undefined) {
        auto size = parameter.textureValue->glTextureSize();
        glUniform2f(parameter.textureSizeUniform, size[0], size[1]);
      }
    }
  }
  if (blitBuffer) {
    auto blitted = blitGlFrameBuffer(blitBuffer, effect.doubleBuffered);
    if (!blitted) return std::unexpected(std::move(blitted.error()));
  }
  auto status = checkGlError("effect switch");
  if (!status) return std::unexpected(std::move(status.error()));
  guard.committed = true;
  return true;
}

std::expected<void, StarException> OpenGlRenderer::setScissorRect(std::optional<RectI> const& scissorRect) {
  if (scissorRect == m_scissorRect)
    return {};

  auto flushed = flushImmediatePrimitives();
  if (!flushed) return std::unexpected(std::move(flushed.error()));

  auto applyScissor = [](std::optional<RectI> const& rect) {
    if (rect) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(rect->xMin(), rect->yMin(), rect->width(), rect->height());
    } else {
      glDisable(GL_SCISSOR_TEST);
    }
  };
  applyScissor(scissorRect);
  auto status = checkGlError("scissor update");
  if (!status) {
    applyScissor(m_scissorRect);
    return std::unexpected(std::move(status.error()));
  }
  m_scissorRect = scissorRect;
  return {};
}

std::optional<RectI> OpenGlRenderer::scissorRect() const {
  return m_scissorRect;
}

std::expected<TexturePtr, StarException> OpenGlRenderer::createTexture(Image const& texture, TextureAddressing addressing, TextureFiltering filtering) {
  auto created = createGlTexture(texture, addressing, filtering);
  if (!created) return std::unexpected(std::move(created.error()));
  return TexturePtr(std::move(*created));
}

void OpenGlRenderer::setSizeLimitEnabled(bool enabled) {
  m_limitTextureGroupSize = enabled;
}

void OpenGlRenderer::setMultiTexturingEnabled(bool enabled) {
  m_useMultiTexturing = enabled;
}

std::expected<void, StarException> OpenGlRenderer::setMultiSampling(unsigned multiSampling) {
  if (m_multiSampling == multiSampling)
    return {};

  bool supportsSampleShading = GLEW_VERSION_4_0 || GLEW_ARB_sample_shading;
  if (multiSampling && !supportsSampleShading)
    return std::unexpected(RendererException("OpenGL sample shading is unavailable"));
  if (multiSampling) {
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    auto queried = checkGlError("multisampling capability query");
    if (!queried) return std::unexpected(std::move(queried.error()));
    if (multiSampling > static_cast<unsigned>(std::max(maxSamples, 0)))
      return std::unexpected(RendererException::format("Requested {} samples, but OpenGL supports {}", multiSampling, maxSamples));
  }
  unsigned previous = m_multiSampling;
  auto applySampling = [&](unsigned samples) {
    if (samples) {
      glEnable(GL_MULTISAMPLE);
      glEnable(GL_SAMPLE_SHADING);
      glMinSampleShading(1.f);
    } else {
      if (supportsSampleShading) {
        glMinSampleShading(0.f);
        glDisable(GL_SAMPLE_SHADING);
      }
      glDisable(GL_MULTISAMPLE);
    }
  };
  applySampling(multiSampling);
  auto status = checkGlError("multisampling setting");
  if (!status) {
    applySampling(previous);
    return std::unexpected(std::move(status.error()));
  }
  m_multiSampling = multiSampling;
  auto configured = loadConfig(m_config);
  if (!configured) {
    m_multiSampling = previous;
    applySampling(previous);
    return std::unexpected(std::move(configured.error()));
  }
  return {};
}

std::expected<void, StarException> OpenGlRenderer::setMainHDR(bool enabled) {
  if (m_hdrSetting == enabled)
    return {};
  
  bool previous = m_hdrSetting;
  m_hdrSetting = enabled;
  auto configured = loadConfig(m_config);
  if (!configured) {
    m_hdrSetting = previous;
    return std::unexpected(std::move(configured.error()));
  }
  return {};
}

std::expected<std::shared_ptr<TextureGroup>, StarException> OpenGlRenderer::createTextureGroup(TextureGroupSize textureSize, TextureFiltering filtering) {
  int maxTextureSize;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
  auto status = checkGlError("texture group capability query");
  if (!status) return std::unexpected(std::move(status.error()));
  if (maxTextureSize <= 0)
    return std::unexpected(RendererException("OpenGL reported an invalid maximum texture size"));
  maxTextureSize = std::min(maxTextureSize, (2 << 14));
  // Large texture sizes are not always supported
  if (textureSize == TextureGroupSize::Large && (m_limitTextureGroupSize || maxTextureSize < 4096))
    textureSize = TextureGroupSize::Medium;

  unsigned atlasNumCells;
  if (textureSize == TextureGroupSize::Large)
    atlasNumCells = 256;
  else if (textureSize == TextureGroupSize::Medium)
    atlasNumCells = 128;
  else // TextureGroupSize::Small
    atlasNumCells = 64;

  auto logged = m_logger.info("detected supported OpenGL texture size {}, using atlasNumCells {}", maxTextureSize, atlasNumCells);
  if (!logged) return std::unexpected(std::move(logged.error()));

  auto glTextureGroup = std::make_shared<GlTextureGroup>(atlasNumCells);
  glTextureGroup->textureAtlasSet.textureFiltering = filtering;
  m_liveTextureGroups.push_back(glTextureGroup);
  return glTextureGroup;
}

std::expected<std::shared_ptr<RenderBuffer>, StarException> OpenGlRenderer::createRenderBuffer() {
  auto buffer = createGlRenderBuffer();
  if (!buffer) return std::unexpected(std::move(buffer.error()));
  return std::shared_ptr<RenderBuffer>(std::move(*buffer));
}

List<RenderPrimitive>& OpenGlRenderer::immediatePrimitives() {
  return m_immediatePrimitives;
}

void OpenGlRenderer::render(RenderPrimitive primitive) { m_immediatePrimitives.push_back(std::move(primitive)); }

std::expected<void, StarException> OpenGlRenderer::appendImmediate(DetachedRenderGeometry const& geometry) {
  auto legacy = m_immediateRenderBuffer->append(m_immediatePrimitives);
  m_immediatePrimitives.clear();
  if (!legacy) {
    m_immediateRenderBuffer->reset();
    return std::unexpected(std::move(legacy.error()));
  }
  auto appended = m_immediateRenderBuffer->append(geometry);
  if (!appended) {
    m_immediateRenderBuffer->reset();
    return std::unexpected(std::move(appended.error()));
  }
  return {};
}

std::expected<void, StarException> OpenGlRenderer::setRenderBuffer(std::shared_ptr<RenderBuffer> const& renderBuffer, DetachedRenderGeometry const& geometry) {
  if (!renderBuffer)
    return std::unexpected(RendererException("Cannot set a null render buffer"));
  auto buffer = rconvert<GlRenderBuffer>(*renderBuffer);
  if (!buffer) return std::unexpected(std::move(buffer.error()));
  return buffer->get().set(geometry);
}

std::expected<void, StarException> OpenGlRenderer::renderBuffer(std::shared_ptr<RenderBuffer> const& renderBuffer, Mat3F const& transformation) {
  if (!renderBuffer)
    return std::unexpected(RendererException("Cannot render a null render buffer"));
  auto buffer = rconvert<GlRenderBuffer>(*renderBuffer);
  if (!buffer) return std::unexpected(std::move(buffer.error()));
  auto flushed = flushImmediatePrimitives();
  if (!flushed) return std::unexpected(std::move(flushed.error()));
  return renderGlBuffer(buffer->get(), transformation);
}

std::expected<void, StarException> OpenGlRenderer::flush(Mat3F const& transformation) {
  return flushImmediatePrimitives(transformation);
}

std::expected<void, StarException> OpenGlRenderer::setScreenSize(Vec2U screenSize) {
  struct ResizeGuard {
    OpenGlRenderer& renderer;
    Vec2U previous;
    GLint viewport[4]{};
    bool committed = false;
    ~ResizeGuard() {
      if (committed) return;
      renderer.m_screenSize = previous;
      glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
      auto framebuffer = renderer.m_currentFrameBuffer;
      auto size = framebuffer
          ? (framebuffer->overrideSize ? *framebuffer->overrideSize : previous / framebuffer->sizeDiv)
          : previous;
      glUniform2f(renderer.m_screenSizeUniform, size[0], size[1]);
    }
  } guard{*this, m_screenSize};
  glGetIntegerv(GL_VIEWPORT, guard.viewport);
  m_screenSize = screenSize;
  glViewport(0, 0, m_screenSize[0], m_screenSize[1]);
  glUniform2f(m_screenSizeUniform, m_screenSize[0], m_screenSize[1]);
  GlTextureBindingGuard textureBinding;
  GlFramebufferBindingGuard framebufferBinding;

  for (auto&& frameBuffer : m_frameBuffers) {
    if (frameBuffer.second->overrideSize) {
      continue; // don't resize this one
    }
    unsigned sizeDiv = frameBuffer.second->sizeDiv;
    bool hdr = settingModeValue(frameBuffer.second->hdrMode,m_hdrSetting);
    if (unsigned multisample = frameBuffer.second->multisample) {
      auto internalFormat =  hdr ? GL_RGBA16F : GL_RGBA8;
      
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, frameBuffer.second->texture->glTextureId());
      glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample, internalFormat, m_screenSize[0] / sizeDiv, m_screenSize[1] / sizeDiv, GL_TRUE);
      if (frameBuffer.second->hasAlt) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, frameBuffer.second->altTexture->glTextureId());
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample, internalFormat, m_screenSize[0] / sizeDiv, m_screenSize[1] / sizeDiv, GL_TRUE);
      }
    } else {
      auto format = frameBuffer.second->alpha ? GL_RGBA : GL_RGB;
      auto internalFormat =  hdr ? 
          (frameBuffer.second->alpha ? GL_RGBA16F : GL_RGB16F) :
          (frameBuffer.second->alpha ? GL_RGBA8 : GL_RGB8);
      auto type = hdr ? GL_FLOAT : GL_UNSIGNED_BYTE;
      
      glBindTexture(GL_TEXTURE_2D, frameBuffer.second->texture->glTextureId());
      glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_screenSize[0] / sizeDiv, m_screenSize[1] / sizeDiv, 0, format, type, NULL);
      if (frameBuffer.second->hasAlt) {
        glBindTexture(GL_TEXTURE_2D, frameBuffer.second->altTexture->glTextureId());
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_screenSize[0] / sizeDiv, m_screenSize[1] / sizeDiv, 0, format, type, NULL);
      }
    }
    auto resized = checkGlError("framebuffer resize");
    if (!resized) return std::unexpected(std::move(resized.error()));
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer.second->id);
    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
      return std::unexpected(RendererException::format("OpenGL framebuffer is not complete after resize: {}", status));
    if (frameBuffer.second->hasAlt) {
      glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer.second->altId);
      status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
        return std::unexpected(RendererException::format("OpenGL alternate framebuffer is not complete after resize: {}", status));
    }
    Vec2U textureSize = m_screenSize / sizeDiv;
    frameBuffer.second->texture->textureSize = textureSize;
    if (frameBuffer.second->hasAlt)
      frameBuffer.second->altTexture->textureSize = textureSize;
  }
  auto status = checkGlError("screen resize");
  if (!status) return std::unexpected(std::move(status.error()));
  guard.committed = true;
  return {};
}

std::expected<void, StarException> OpenGlRenderer::startFrame() {
  if (m_scissorRect)
    glDisable(GL_SCISSOR_TEST);

  for (auto&& frameBuffer : m_frameBuffers) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.second->id);
    if (frameBuffer.second->clear)
      glClear(GL_COLOR_BUFFER_BIT);
    
    if (frameBuffer.second->hasAlt) {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.second->altId);
      if (frameBuffer.second->clear)
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    frameBuffer.second->blitted = false;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glClear(GL_COLOR_BUFFER_BIT);

  if (m_scissorRect)
    glEnable(GL_SCISSOR_TEST);
  return checkGlError("frame start");
}

std::expected<void, StarException> OpenGlRenderer::finishFrame() {
  auto flushed = flushImmediatePrimitives();
  if (!flushed) return std::unexpected(std::move(flushed.error()));
  // Release immediate-buffer texture locks before moving atlas entries.
  m_immediateRenderBuffer->reset();
  for (auto& group : m_liveTextureGroups) {
    if (group.use_count() != 1 || group->textureAtlasSet.totalTextures() > 0) {
      auto compressed = group->textureAtlasSet.compressionPass(1);
      if (!compressed) return std::unexpected(std::move(compressed.error()));
    }
  }
  filter(m_liveTextureGroups, [](auto const& group) {
    return group.use_count() != 1 || group->textureAtlasSet.totalTextures() > 0;
  });
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return checkGlError("frame finish");
}

OpenGlRenderer::GlTextureAtlasSet::GlTextureAtlasSet(unsigned atlasNumCells)
  : TextureAtlasSet(16, atlasNumCells) {}

std::expected<GLuint, StarException> OpenGlRenderer::GlTextureAtlasSet::createAtlasTexture(Vec2U const& size, PixelFormat pixelFormat) {
  GlTextureBindingGuard binding;
  GLuint glTextureId = 0;
  glGenTextures(1, &glTextureId);
  auto created = checkGlError("atlas texture creation");
  if (!created) {
    if (glTextureId) glDeleteTextures(1, &glTextureId);
    return std::unexpected(std::move(created.error()));
  }
  if (glTextureId == 0)
    return std::unexpected(RendererException("Could not generate texture in OpenGlRenderer::TextureGroup::createAtlasTexture()"));

  glBindTexture(GL_TEXTURE_2D, glTextureId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (textureFiltering == TextureFiltering::Nearest) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  auto uploaded = uploadTextureImage(pixelFormat, size, nullptr);
  if (!uploaded) {
    glDeleteTextures(1, &glTextureId);
    return std::unexpected(std::move(uploaded.error()));
  }
  return glTextureId;
}

void OpenGlRenderer::GlTextureAtlasSet::destroyAtlasTexture(GLuint const& glTexture) {
  glDeleteTextures(1, &glTexture);
}

std::expected<void, StarException> OpenGlRenderer::GlTextureAtlasSet::copyAtlasPixels(
    GLuint const& glTexture, Vec2U const& bottomLeft, Image const& image) {
  GlTextureBindingGuard binding;
  glBindTexture(GL_TEXTURE_2D, glTexture);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  GLenum format;
  auto pixelFormat = image.pixelFormat();
  if (pixelFormat == PixelFormat::RGB24)
    format = GL_RGB;
  else if (pixelFormat == PixelFormat::RGBA32)
    format = GL_RGBA;
  else if (pixelFormat == PixelFormat::BGR24)
    format = GL_BGR;
  else if (pixelFormat == PixelFormat::BGRA32)
    format = GL_BGRA;
  else
    return std::unexpected(RendererException("Unsupported texture format in OpenGlRenderer::TextureGroup::copyAtlasPixels"));

  glTexSubImage2D(GL_TEXTURE_2D, 0, bottomLeft[0], bottomLeft[1], image.width(), image.height(), format, GL_UNSIGNED_BYTE, image.data().data());
  return checkGlError("atlas pixel upload");
}

OpenGlRenderer::GlTextureGroup::GlTextureGroup(unsigned atlasNumCells)
  : textureAtlasSet(atlasNumCells) {}

OpenGlRenderer::GlTextureGroup::~GlTextureGroup() {
  textureAtlasSet.reset();
}

TextureFiltering OpenGlRenderer::GlTextureGroup::filtering() const {
  return textureAtlasSet.textureFiltering;
}

std::expected<TexturePtr, StarException> OpenGlRenderer::GlTextureGroup::create(Image const& texture) {
  // If the image is empty, or would not fit in the texture atlas with border
  // pixels, just create a regular texture
  Vec2U atlasTextureSize = textureAtlasSet.atlasTextureSize();
  if (texture.empty() || texture.width() + 2 > atlasTextureSize[0] || texture.height() + 2 > atlasTextureSize[1]) {
    auto lone = createGlTexture(texture, TextureAddressing::Clamp, textureAtlasSet.textureFiltering);
    if (!lone) return std::unexpected(std::move(lone.error()));
    return TexturePtr(std::move(*lone));
  }

  auto glGroupedTexture = make_ref<GlGroupedTexture>();
  glGroupedTexture->parentGroup = shared_from_this();
  auto atlasTexture = textureAtlasSet.addTexture(texture);
  if (!atlasTexture) return std::unexpected(std::move(atlasTexture.error()));
  glGroupedTexture->parentAtlasTexture = std::move(*atlasTexture);

  return glGroupedTexture;
}

OpenGlRenderer::GlGroupedTexture::~GlGroupedTexture() {
  if (parentAtlasTexture) {
    auto freed = parentGroup->textureAtlasSet.freeTexture(parentAtlasTexture);
    if (!freed)
      EmergencyLog::error("Error freeing grouped texture: {}", freed.error().what());
  }
}

Vec2U OpenGlRenderer::GlGroupedTexture::size() const {
  return parentAtlasTexture->imageSize();
}

TextureFiltering OpenGlRenderer::GlGroupedTexture::filtering() const {
  return parentGroup->filtering();
}

TextureAddressing OpenGlRenderer::GlGroupedTexture::addressing() const {
  return TextureAddressing::Clamp;
}

GLuint OpenGlRenderer::GlGroupedTexture::glTextureId() const {
  return parentAtlasTexture->atlasTexture();
}

Vec2U OpenGlRenderer::GlGroupedTexture::glTextureSize() const {
  return parentGroup->textureAtlasSet.atlasTextureSize();
}

Vec2U OpenGlRenderer::GlGroupedTexture::glTextureCoordinateOffset() const {
  return parentAtlasTexture->atlasTextureCoordinates().min();
}

void OpenGlRenderer::GlGroupedTexture::incrementBufferUseCount() {
  if (bufferUseCount == 0)
    parentAtlasTexture->setLocked(true);
  ++bufferUseCount;
}

void OpenGlRenderer::GlGroupedTexture::decrementBufferUseCount() {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((bufferUseCount != 0));
  }
  if (bufferUseCount == 1)
    parentAtlasTexture->setLocked(false);
  --bufferUseCount;
}

OpenGlRenderer::GlLoneTexture::~GlLoneTexture() {
  if (textureId != 0)
    glDeleteTextures(1, &textureId);
}

Vec2U OpenGlRenderer::GlLoneTexture::size() const {
  return textureSize;
}

TextureFiltering OpenGlRenderer::GlLoneTexture::filtering() const {
  return textureFiltering;
}

TextureAddressing OpenGlRenderer::GlLoneTexture::addressing() const {
  return textureAddressing;
}

GLuint OpenGlRenderer::GlLoneTexture::glTextureId() const {
  return textureId;
}

Vec2U OpenGlRenderer::GlLoneTexture::glTextureSize() const {
  return textureSize;
}

Vec2U OpenGlRenderer::GlLoneTexture::glTextureCoordinateOffset() const {
  return Vec2U();
}


OpenGlRenderer::GlRenderBuffer::~GlRenderBuffer() {
  for (auto const& texture : usedTextures) {
    if (auto gt = asRef<GlGroupedTexture>(*texture))
      gt->get().decrementBufferUseCount();
  }
  for (auto const& vb : vertexBuffers)
    glDeleteBuffers(1, &vb.vertexBuffer);
  glDeleteVertexArrays(1, &vertexArray);
}

void OpenGlRenderer::GlRenderBuffer::reset() {
  for (auto const& texture : usedTextures)
    if (auto grouped = asRef<GlGroupedTexture>(*texture))
      grouped->get().decrementBufferUseCount();
  usedTextures.clear();
  accumulationBuffer.clear();
  packedBatches.clear();
  for (auto& vb : vertexBuffers)
    vb.vertexCount = 0;
}

template <typename VertexAt>
std::expected<void, StarException> OpenGlRenderer::GlRenderBuffer::appendPrimitive(std::uint8_t kind, std::size_t count, TexturePtr const& texture, VertexAt const& vertexAt) {
  if (kind == 2 && count <= 2)
    return {};

  TexturePtr const& selectedTexture = texture ? texture : whiteTexture;
  auto glTexture = asRef<GlTexture>(*selectedTexture);
  if (!glTexture)
    return std::unexpected(RendererException("Render primitive uses a texture from another renderer"));

  auto textureId = glTexture->get().glTextureId();
  auto textureLimit = useMultiTexturing ? MultiTextureCount : 1;
  if (packedBatches.empty())
    packedBatches.emplace_back();
  auto findTexture = [&](PackedBatch const& batch) {
    std::size_t i = 0;
    while (i < batch.textureCount && batch.textures[i].texture != textureId)
      ++i;
    return i;
  };
  auto textureIndex = findTexture(packedBatches.back());
  if (textureIndex == packedBatches.back().textureCount && textureIndex >= textureLimit) {
    packedBatches.emplace_back();
    packedBatches.back().firstVertex = accumulationBuffer.size();
    textureIndex = 0;
  }
  auto& batch = packedBatches.back();
  if (textureIndex == batch.textureCount) {
    batch.textures[textureIndex] = {textureId, glTexture->get().glTextureSize()};
    ++batch.textureCount;
  }
  if (usedTextures.add(selectedTexture))
    if (auto grouped = asRef<GlGroupedTexture>(*selectedTexture))
      grouped->get().incrementBufferUseCount();

  auto textureOffset = Vec2F(glTexture->get().glTextureCoordinateOffset());
  auto outputCount = kind == 0 ? 3 : kind == 1 ? 6 : (count - 2) * 3;
  auto offset = accumulationBuffer.size();
  accumulationBuffer.resize(offset + outputCount);
  auto appendVertex = [&](std::size_t index, std::size_t prev, std::size_t next) {
    auto const& vertex = vertexAt(index);
    auto& glv = accumulationBuffer[offset++];
    glv.pos = vertex.screenCoordinate;
    glv.uv = vertex.textureCoordinate + textureOffset;
    glv.color = vertex.color;
    glv.pack.vars.textureIndex = static_cast<std::uint8_t>(textureIndex);
    glv.pack.vars.fullbright = vertex.param1 > 0.0f;
    // Use the original outside neighbors, not a quad's triangulation diagonal.
    glv.pack.vars.rX = std::min(std::abs(glv.pos.x() - vertexAt(prev).screenCoordinate.x()), std::abs(glv.pos.x() - vertexAt(next).screenCoordinate.x())) < 0.001f;
    glv.pack.vars.rY = std::min(std::abs(glv.pos.y() - vertexAt(prev).screenCoordinate.y()), std::abs(glv.pos.y() - vertexAt(next).screenCoordinate.y())) < 0.001f;
    glv.pack.vars.unused = 0;
  };
  if (kind == 0) {
    appendVertex(0, 2, 1);
    appendVertex(1, 0, 2);
    appendVertex(2, 1, 0);
  } else if (kind == 1) {
    appendVertex(0, 3, 1);
    appendVertex(1, 0, 2);
    appendVertex(2, 1, 3);
    appendVertex(0, 3, 1);
    appendVertex(2, 1, 3);
    appendVertex(3, 2, 0);
  } else {
    for (std::size_t i = 1; i < count - 1; ++i) {
      appendVertex(0, i + 1, i);
      appendVertex(i, 0, i + 1);
      appendVertex(i + 1, i, 0);
    }
  }
  batch.vertexCount += outputCount;
  return {};
}

std::expected<void, StarException> OpenGlRenderer::GlRenderBuffer::append(List<RenderPrimitive> const& primitives) {
  for (auto const& primitive : primitives) {
    auto appended = std::visit([&](auto const& shape) -> std::expected<void, StarException> {
      using T = std::remove_cvref_t<decltype(shape)>;
      if constexpr (std::is_same_v<T, RenderPoly>) {
        return appendPrimitive(2, shape.vertexes.size(), shape.texture, [&](std::size_t i) -> RenderVertex const& { return shape.vertexes[i]; });
      } else {
        constexpr bool quad = std::is_same_v<T, RenderQuad>;
        return appendPrimitive(quad ? 1 : 0, quad ? 4 : 3, shape.texture, [&](std::size_t i) -> RenderVertex const& {
          if (i == 0) return shape.a;
          if (i == 1) return shape.b;
          if constexpr (quad)
            if (i == 3) return shape.d;
          return shape.c;
        });
      }
    }, primitive);
    if (!appended) return std::unexpected(std::move(appended.error()));
  }
  return {};
}

std::expected<void, StarException> OpenGlRenderer::GlRenderBuffer::append(DetachedRenderGeometry const& geometry) {
  for (auto const& primitive : geometry.primitives) {
    if (primitive.kind > 2 || (primitive.kind == 0 && primitive.count != 3) || (primitive.kind == 1 && primitive.count != 4))
      return std::unexpected(RendererException("Invalid detached render primitive"));
    // Empty polygon fans never inspect their texture, just like legacy polys.
    if (primitive.kind == 2 && primitive.count <= 2)
      continue;
    if (primitive.first > geometry.vertices.size() || primitive.count > geometry.vertices.size() - primitive.first)
      return std::unexpected(RendererException("Detached render primitive vertices are out of range"));
    auto texture = std::cref(whiteTexture);
    if (primitive.texture) {
      auto found = geometry.textures.find(primitive.texture);
      if (found == geometry.textures.end() || !found->second)
        return std::unexpected(RendererException("Detached render primitive uses an unknown texture"));
      texture = std::cref(found->second);
    }
    auto appended = appendPrimitive(primitive.kind, primitive.count, texture.get(),
        [&](std::size_t i) -> RenderVertex const& { return geometry.vertices[primitive.first + i]; });
    if (!appended) return std::unexpected(std::move(appended.error()));
  }
  return {};
}

std::expected<void, StarException> OpenGlRenderer::GlRenderBuffer::set(List<RenderPrimitive>& primitives) {
  reset();
  auto appended = append(primitives);
  if (!appended) {
    reset();
    return std::unexpected(std::move(appended.error()));
  }
  return upload();
}

std::expected<void, StarException> OpenGlRenderer::GlRenderBuffer::set(DetachedRenderGeometry const& geometry) {
  reset();
  auto appended = append(geometry);
  if (!appended) {
    reset();
    return std::unexpected(std::move(appended.error()));
  }
  return upload();
}

std::expected<void, StarException> OpenGlRenderer::GlRenderBuffer::upload() {
  struct BufferGuard {
    GlRenderBuffer& buffer;
    GLint vertexArray = 0;
    GLint arrayBuffer = 0;
    bool committed = false;
    ~BufferGuard() {
      if (!committed) {
        for (auto const& vb : buffer.vertexBuffers)
          glDeleteBuffers(1, &vb.vertexBuffer);
        buffer.vertexBuffers.clear();
        buffer.reset();
      }
      glBindVertexArray(vertexArray);
      glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
    }
  } guard{*this};
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &guard.vertexArray);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &guard.arrayBuffer);
  glBindVertexArray(vertexArray);
  vertexBuffers.reserve(packedBatches.size());
  for (std::size_t index = 0; index < packedBatches.size(); ++index) {
    auto const& batch = packedBatches[index];
    if (index == vertexBuffers.size())
      vertexBuffers.emplace_back();
    auto& vb = vertexBuffers[index];
    vb.textureCount = batch.textureCount;
    for (std::size_t i = 0; i < batch.textureCount; ++i)
      vb.textures[i] = batch.textures[i];
    vb.vertexCount = batch.vertexCount;
    if (!vb.vertexBuffer) {
      glGenBuffers(1, &vb.vertexBuffer);
      auto created = checkGlError("vertex buffer creation");
      if (!created) return std::unexpected(std::move(created.error()));
      if (!vb.vertexBuffer)
        return std::unexpected(RendererException("Could not create OpenGL vertex buffer"));
    }
    glBindBuffer(GL_ARRAY_BUFFER, vb.vertexBuffer);
    auto vertices = std::span(accumulationBuffer).subspan(batch.firstVertex, batch.vertexCount);
    if (vb.vertexCapacity >= vb.vertexCount) {
      glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size_bytes(), vertices.data());
    } else {
      glBufferData(GL_ARRAY_BUFFER, vertices.size_bytes(), vertices.data(), GL_STREAM_DRAW);
      vb.vertexCapacity = vb.vertexCount;
    }
    auto status = checkGlError("vertex buffer upload");
    if (!status) return std::unexpected(std::move(status.error()));
  }
  while (vertexBuffers.size() > packedBatches.size()) {
    glDeleteBuffers(1, &vertexBuffers.back().vertexBuffer);
    vertexBuffers.pop_back();
  }
  accumulationBuffer.clear();
  packedBatches.clear();
  guard.committed = true;
  return {};
}

bool OpenGlRenderer::logGlErrorSummary(String prefix) {
  if (GLenum error = glGetError()) {
    EmergencyLog::error("{}: ", prefix);
    do {
      if (error == GL_INVALID_ENUM) {
        EmergencyLog::error("GL_INVALID_ENUM");
      } else if (error == GL_INVALID_VALUE) {
        EmergencyLog::error("GL_INVALID_VALUE");
      } else if (error == GL_INVALID_OPERATION) {
        EmergencyLog::error("GL_INVALID_OPERATION");
      } else if (error == GL_INVALID_FRAMEBUFFER_OPERATION) {
        EmergencyLog::error("GL_INVALID_FRAMEBUFFER_OPERATION");
      } else if (error == GL_OUT_OF_MEMORY) {
        EmergencyLog::error("GL_OUT_OF_MEMORY");
      } else if (error == GL_STACK_UNDERFLOW) {
        EmergencyLog::error("GL_STACK_UNDERFLOW");
      } else if (error == GL_STACK_OVERFLOW) {
        EmergencyLog::error("GL_STACK_OVERFLOW");
      } else {
        EmergencyLog::error("<UNRECOGNIZED GL ERROR>");
      }
    } while ((error = glGetError()));
    return true;
  }
  return false;
}

std::expected<void, StarException> OpenGlRenderer::checkGlError(std::string_view operation) {
  GLenum first = glGetError();
  if (first == GL_NO_ERROR)
    return {};
  while (glGetError() != GL_NO_ERROR) {}
  return std::unexpected(RendererException::format("OpenGL error {} during {}", first, operation));
}

std::expected<void, StarException> OpenGlRenderer::uploadTextureImage(PixelFormat pixelFormat, Vec2U size, std::uint8_t const* data) {
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  std::optional<GLenum> internalFormat;
  GLenum format;
  GLenum type = GL_UNSIGNED_BYTE;
  if (pixelFormat == PixelFormat::RGB24)
    format = GL_RGB;
  else if (pixelFormat == PixelFormat::RGBA32)
    format = GL_RGBA;
  else if (pixelFormat == PixelFormat::BGR24)
    format = GL_BGR;
  else if (pixelFormat == PixelFormat::BGRA32)
    format = GL_BGRA;
  else {
    type = GL_FLOAT;
    if (pixelFormat == PixelFormat::RGB_F) {
      internalFormat = GL_RGB32F;
      format = GL_RGB;
    } else if (pixelFormat == PixelFormat::RGBA_F) {
      internalFormat = GL_RGBA32F;
      format = GL_RGBA;
    } else
      return std::unexpected(RendererException("Unsupported texture format in OpenGlRenderer::uploadTextureImage"));
  }

  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat.value_or(format), size[0], size[1], 0, format, type, data);
  return checkGlError("texture image upload");
}

std::expected<void, StarException> OpenGlRenderer::flushImmediatePrimitives(Mat3F const& transformation) {
  auto appended = m_immediateRenderBuffer->append(m_immediatePrimitives);
  m_immediatePrimitives.clear();
  if (!appended) {
    m_immediateRenderBuffer->reset();
    return std::unexpected(std::move(appended.error()));
  }
  if (m_immediateRenderBuffer->packedBatches.empty())
    return {};
  auto prepared = m_immediateRenderBuffer->upload();
  if (!prepared) return std::unexpected(std::move(prepared.error()));
  auto rendered = renderGlBuffer(*m_immediateRenderBuffer, transformation);
  m_immediateRenderBuffer->reset();
  if (!rendered) return std::unexpected(std::move(rendered.error()));
  return {};
}

auto OpenGlRenderer::createGlTexture(ImageView const& image, TextureAddressing addressing, TextureFiltering filtering)
    -> std::expected<RefPtr<GlLoneTexture>, StarException> {
  GlTextureBindingGuard binding;
  auto glLoneTexture = make_ref<GlLoneTexture>();
  glLoneTexture->textureFiltering = filtering;
  glLoneTexture->textureAddressing = addressing;
  glLoneTexture->textureSize = image.size;

  glGenTextures(1, &glLoneTexture->textureId);
  auto created = checkGlError("texture handle creation");
  if (!created) return std::unexpected(std::move(created.error()));
  if (glLoneTexture->textureId == 0)
    return std::unexpected(RendererException("Could not generate texture in OpenGlRenderer::createGlTexture"));

  glBindTexture(GL_TEXTURE_2D, glLoneTexture->textureId);

  if (addressing == TextureAddressing::Clamp) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }

  if (filtering == TextureFiltering::Nearest) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }


  if (!image.empty()) {
    auto uploaded = uploadTextureImage(image.format, image.size, image.data.data());
    if (!uploaded) return std::unexpected(std::move(uploaded.error()));
  }
  auto status = checkGlError("texture creation");
  if (!status) return std::unexpected(std::move(status.error()));

  return glLoneTexture;
}

auto OpenGlRenderer::createGlRenderBuffer() -> std::expected<std::shared_ptr<GlRenderBuffer>, StarException> {
  auto glrb = std::make_shared<GlRenderBuffer>();
  glGenVertexArrays(1, &glrb->vertexArray);
  auto created = checkGlError("vertex array creation");
  if (!created) return std::unexpected(std::move(created.error()));
  if (!glrb->vertexArray)
    return std::unexpected(RendererException("Could not create OpenGL vertex array"));
  auto status = checkGlError("render buffer creation");
  if (!status) return std::unexpected(std::move(status.error()));
  glrb->whiteTexture = m_whiteTexture;
  glrb->useMultiTexturing = m_useMultiTexturing;
  return glrb;
}

std::expected<void, StarException> OpenGlRenderer::renderGlBuffer(GlRenderBuffer const& renderBuffer, Mat3F const& transformation) {
  struct DrawBindingGuard {
    GLint vertexArray = 0;
    GLint arrayBuffer = 0;
    GLint activeTexture = 0;
    DrawBindingGuard() {
      glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
      glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    }
    ~DrawBindingGuard() {
      glBindVertexArray(vertexArray);
      glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
      glActiveTexture(activeTexture);
    }
  } binding;
  glBindVertexArray(renderBuffer.vertexArray);
  for (auto const& vb : renderBuffer.vertexBuffers) {
    if (vb.vertexCount == 0)
      continue;
    glUniformMatrix3fv(m_vertexTransformUniform, 1, GL_TRUE, transformation.ptr());

    if (m_currentEffect->get().includeVBTextures) {
      for (std::size_t i = 0; i < vb.textureCount; ++i) {
        glUniform2f(m_textureSizeUniforms[i], vb.textures[i].size[0], vb.textures[i].size[1]);
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, vb.textures[i].texture);
      }
    }

    for (auto const& p : m_currentEffect->get().textures) {
      if (p.second.textureValue) {
        glActiveTexture(GL_TEXTURE0 + p.second.textureUnit);
        glBindTexture(GL_TEXTURE_2D, p.second.textureValue->textureId);
      }
    }

    glBindBuffer(GL_ARRAY_BUFFER, vb.vertexBuffer);

    // check if these exist (might not in mod shaders) so we don't get a bunch of useless errors
    if (m_positionAttribute != -1)
      glEnableVertexAttribArray(m_positionAttribute);
    if (m_texCoordAttribute != -1)
      glEnableVertexAttribArray(m_texCoordAttribute);
    if (m_colorAttribute != -1)
      glEnableVertexAttribArray(m_colorAttribute);
    if (m_dataAttribute != -1)
      glEnableVertexAttribArray(m_dataAttribute);

    if (m_positionAttribute != -1)
      glVertexAttribPointer(m_positionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(GlRenderVertex),
                            reinterpret_cast<GLvoid*>(offsetof(GlRenderVertex, pos)));
    if (m_texCoordAttribute != -1)
      glVertexAttribPointer(m_texCoordAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(GlRenderVertex),
                            reinterpret_cast<GLvoid*>(offsetof(GlRenderVertex, uv)));
    if (m_colorAttribute != -1)
      glVertexAttribPointer(m_colorAttribute, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GlRenderVertex),
                            reinterpret_cast<GLvoid*>(offsetof(GlRenderVertex, color)));
    if (m_dataAttribute != -1)
      glVertexAttribIPointer(m_dataAttribute, 1, GL_INT, sizeof(GlRenderVertex),
                             reinterpret_cast<GLvoid*>(offsetof(GlRenderVertex, pack)));

    glDrawArrays(GL_TRIANGLES, 0, vb.vertexCount);
  }
  return checkGlError("buffer draw");
}

//Assumes the passed effect program is currently in use.
void OpenGlRenderer::setupGlUniforms(Effect& effect, Vec2U screenSize) {
  m_positionAttribute = effect.getAttribute("vertexPosition");
  m_colorAttribute = effect.getAttribute("vertexColor");
  m_texCoordAttribute = effect.getAttribute("vertexTextureCoordinate");
  m_dataAttribute = effect.getAttribute("vertexData");

  m_textureUniforms.clear();
  m_textureSizeUniforms.clear();
  if (effect.includeVBTextures) {
    for (std::size_t i = 0; i < MultiTextureCount; ++i) {
      m_textureUniforms.push_back(effect.getUniform(Star::format("texture{}", i)));
      m_textureSizeUniforms.push_back(effect.getUniform(Star::format("textureSize{}", i)));
    }
  }
  m_screenSizeUniform = effect.getUniform("screenSize");
  m_vertexTransformUniform = effect.getUniform("vertexTransform");

  if (effect.includeVBTextures) {
    for (std::size_t i = 0; i < MultiTextureCount; ++i)
      glUniform1i(m_textureUniforms[i], i);
  }
  for (auto const& texture : effect.textures)
    glUniform1i(texture.second.textureUniform, texture.second.textureUnit);

  glUniform2f(m_screenSizeUniform, screenSize[0], screenSize[1]);
  
  auto uploadParameters = [](auto const& parameters) {
    for (auto const& param : parameters) {
      auto const& parameter = param.second;
      if (!parameter.parameterValue)
        continue;
      uploadEffectParameter(parameter.parameterUniform, *parameter.parameterValue);
    }
  };
  uploadParameters(effect.parameters);
  uploadParameters(effect.scriptables);
}

std::expected<RefPtr<OpenGlRenderer::GlFrameBuffer>, StarException> OpenGlRenderer::getGlFrameBuffer(String const& id) {
  if (auto framebuffer = m_frameBuffers.find_ref(id))
    return framebuffer->get();
  else
    return std::unexpected(RendererException::format("Frame buffer '{}' does not exist", id));
}

std::expected<void, StarException> OpenGlRenderer::blitGlFrameBuffer(RefPtr<GlFrameBuffer> const& frameBuffer, bool const& useAlt) {
  GlFramebufferBindingGuard binding;
  auto& size = m_screenSize;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (useAlt && frameBuffer->hasAlt) ? frameBuffer->altId : frameBuffer->id);
  glBlitFramebuffer(
    0, 0, size[0], size[1],
    0, 0, size[0], size[1],
    GL_COLOR_BUFFER_BIT, GL_NEAREST
  );
  return checkGlError("framebuffer blit");
}

std::expected<void, StarException> OpenGlRenderer::switchGlFrameBuffer(RefPtr<GlFrameBuffer> const& frameBuffer) {
  if (m_currentFrameBuffer == frameBuffer && !frameBuffer->justSwapped)
    return {};
  
  frameBuffer->justSwapped = false;
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->id);
  m_currentFrameBuffer = frameBuffer;
  return checkGlError("framebuffer switch");
}

GLuint OpenGlRenderer::Effect::getAttribute(String const& name) {
  auto find = attributes.find(name);
  if (find == attributes.end()) {
    GLuint attrib = glGetAttribLocation(program, name.utf8_data());
    attributes[name] = attrib;
    return attrib;
  }
  return find->second;
}

GLuint OpenGlRenderer::Effect::getUniform(String const& name) {
  auto find = uniforms.find(name);
  if (find == uniforms.end()) {
    GLuint uniform = glGetUniformLocation(program, name.utf8_data());
    uniforms[name] = uniform;
    return uniform;
  }
  return find->second;
}


}

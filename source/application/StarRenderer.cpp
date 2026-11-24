import std;
import star.format;
import star.json;
import star.list;
import star.map;
import star.image;
import star.poly;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.math_common;

import star.vector;
import star.rect;
namespace Star {

std::uint64_t legacyShaderSourceFingerprint(std::string_view source) noexcept {
  std::uint64_t fingerprint = 14695981039346656037ull;
  for (std::size_t i = 0; i < source.size(); ++i) {
    auto byte = static_cast<unsigned char>(source[i]);
    if (byte == '\r' && i + 1 < source.size() && source[i + 1] == '\n')
      continue;
    fingerprint ^= byte;
    fingerprint *= 1099511628211ull;
  }
  return fingerprint;
}

namespace {
template <std::floating_point T>
std::expected<void, StarException> validateOutputLevels(T exposure, T paperWhite, T peak) {
  if (!is_finite(exposure) || exposure < -10 || exposure > 10)
    return std::unexpected(RendererException("renderExposure must be finite and between -10 and 10 EV"));
  if (!is_finite(paperWhite) || paperWhite < 80 || paperWhite > 500)
    return std::unexpected(RendererException("hdrPaperWhite must be finite and between 80 and 500 nits"));
  if (!is_finite(peak) || peak < paperWhite || peak > 10000)
    return std::unexpected(RendererException("hdrPeakLuminance must be finite, at least hdrPaperWhite and at most 10000 nits"));
  return {};
}
}

std::expected<void, StarException> validateRenderOutputSettings(RenderOutputSettings const& settings) {
  switch (settings.mode) {
    case RenderOutputMode::Legacy:
    case RenderOutputMode::Sdr:
    case RenderOutputMode::Hdr10:
      return validateOutputLevels(settings.exposure, settings.paperWhiteNits, settings.peakNits);
  }
  return std::unexpected(RendererException("renderOutput must be legacy, sdr or hdr10"));
}

std::expected<RenderOutputSettings, StarException> renderOutputSettingsFromJson(Json const& config) {
  auto name = config.get_string("renderOutput");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto mode = enum_value<RenderOutputModeNames>(*name);
  if (!mode)
    return std::unexpected(RendererException("renderOutput must be legacy, sdr or hdr10"));
  auto exposure = config.get_double("renderExposure");
  if (!exposure)
    return std::unexpected(std::move(exposure.error()));
  auto paperWhite = config.get_double("hdrPaperWhite");
  if (!paperWhite)
    return std::unexpected(std::move(paperWhite.error()));
  auto peak = config.get_double("hdrPeakLuminance");
  if (!peak)
    return std::unexpected(std::move(peak.error()));
  if (auto result = validateOutputLevels(*exposure, *paperWhite, *peak); !result)
    return std::unexpected(std::move(result.error()));
  return RenderOutputSettings{*mode, static_cast<float>(*exposure), static_cast<float>(*paperWhite), static_cast<float>(*peak)};
}

std::expected<double, StarException> validatedRenderFpsLimit(Json const& value) {
  auto rate = value.to_double();
  if (!rate)
    return std::unexpected(std::move(rate.error()));
  if (!is_finite(*rate) || *rate < 0 || *rate > std::numeric_limits<float>::max())
    return std::unexpected(RendererException("renderFpsLimit must be finite, nonnegative and representable as a float"));
  return *rate;
}

std::expected<List<std::string_view>, StarException> RenderPreferences::observe(Json const& config) {
  auto snapshot = config.object_ptr();
  if (!snapshot)
    return std::unexpected(std::move(snapshot.error()));
  List<std::string_view> rejected;
  if (*snapshot == m_observed)
    return rejected;

  auto changed = [&](std::string_view key) {
    if (!m_observed)
      return true;
    auto current = (*snapshot)->find(key);
    auto old = m_observed->find(key);
    if (current == (*snapshot)->end() || old == m_observed->end())
      return (current == (*snapshot)->end()) != (old == m_observed->end());
    auto const& value = current->second;
    if (value.type() != old->second.type())
      return true;
    // No preference accepts containers. Do not traverse malformed JSON trees
    // (or re-diagnose one) because an unrelated config key changed.
    if (value.is_type(Json::Type::Array) || value.is_type(Json::Type::Object))
      return false;
    if (value.is_type(Json::Type::Float))
      return std::bit_cast<std::uint64_t>(*value.to_double()) != std::bit_cast<std::uint64_t>(*old->second.to_double());
    return value != old->second;
  };
  auto accept = [&](std::string_view key, auto parsed, auto& destination) {
    if (parsed)
      destination = *parsed;
    else
      rejected.push_back(key);
  };
  if (changed("renderOutput") || changed("renderExposure") || changed("hdrPaperWhite") || changed("hdrPeakLuminance"))
    accept("renderOutput/renderExposure/hdrPaperWhite/hdrPeakLuminance", renderOutputSettingsFromJson(config), output);
  if (changed("renderFpsLimit"))
    accept("renderFpsLimit", config.get("renderFpsLimit").and_then(validatedRenderFpsLimit), fpsLimit);
  if (changed("vsync"))
    accept("vsync", config.get_bool("vsync"), vsync);
  if (changed("hdr"))
    accept("hdr", config.get_bool("hdr", true), hdr);
  m_observed = std::move(*snapshot);
  return rejected;
}


RenderQuad::RenderQuad(Vec2F posA, Vec2F posB, Vec2F posC, Vec2F posD, Vec4B color, float param1) : texture() {
  a = { posA, { 0, 0 }, color, param1 };
  b = { posB, { 0, 0 }, color, param1 };
  c = { posC, { 0, 0 }, color, param1 };
  d = { posD, { 0, 0 }, color, param1 };
}

RenderQuad::RenderQuad(TexturePtr tex, Vec2F minPosition, float textureScale, Vec4B color, float param1) : texture(std::move(tex)) {
  Vec2F size = Vec2F(texture->size());
  a = { minPosition, { 0, 0 }, color, param1};
  b = { { (minPosition[0] + size[0] * textureScale), minPosition[1] }, { size[0], 0 }, color, param1 };
  c = { { (minPosition[0] + size[0] * textureScale), (minPosition[1] + size[1] * textureScale) }, size, color, param1 };
  d = { { minPosition[0], (minPosition[1] + size[1] * textureScale) }, { 0, size[1] }, color, param1 };
}

RenderQuad::RenderQuad(TexturePtr tex, RectF const& screenCoords, Vec4B color, float param1) : texture(std::move(tex)) {
  Vec2F size = Vec2F(texture->size());
  a = { screenCoords.min(), { 0, 0 }, color, param1 };
  b = { { screenCoords.x_max(), screenCoords.y_min(), }, { size[0], 0.f }, color, param1 };
  c = { screenCoords.max(), size, color, param1};
  d = { { screenCoords.x_min(), screenCoords.y_max(), }, { 0.f, size[1] }, color, param1 };
}

RenderQuad::RenderQuad(TexturePtr tex, Vec2F posA, Vec2F uvA, Vec2F posB, Vec2F uvB, Vec2F posC, Vec2F uvC, Vec2F posD, Vec2F uvD, Vec4B color, float param1) : texture(std::move(tex)) {
  a = { posA, uvA, color, param1 };
  b = { posB, uvB, color, param1 };
  c = { posC, uvC, color, param1 };
  d = { posD, uvD, color, param1 };
}

RenderQuad::RenderQuad(TexturePtr tex, RenderVertex vA, RenderVertex vB, RenderVertex vC, RenderVertex vD)
  : texture(std::move(tex)), a(std::move(vA)), b(std::move(vB)), c(std::move(vC)), d(std::move(vD)) {}

RenderQuad::RenderQuad(RectF const& rect, Vec4B color, float param1)
  : a{ rect.min(), {}, color, param1 }
  , b{ { rect.x_max(), rect.y_min()}, {}, color, param1 }
  , c{ rect.max(), {}, color, param1 }
  , d{ { rect.x_min() ,rect.y_max() }, {}, color, param1 } {};


RenderPoly::RenderPoly(List<Vec2F> const& verts, Vec4B color, float param1) {
  vertexes.reserve(verts.size());
  for (Vec2F const& v : verts)
    vertexes.push_back({v, {0, 0}, color, param1});
}

RenderTriangle::RenderTriangle(Vec2F posA, Vec2F posB, Vec2F posC, Vec4B color, float param1) : texture() {
  a = { posA, { 0, 0 }, color, param1 };
  b = { posB, { 0, 0 }, color, param1 };
  c = { posC, { 0, 0 }, color, param1 };
}

RenderTriangle::RenderTriangle(TexturePtr tex, Vec2F posA, Vec2F uvA, Vec2F posB, Vec2F uvB, Vec2F posC, Vec2F uvC, Vec4B color, float param1) : texture(std::move(tex)) {
  a = { posA, uvA, color, param1 };
  b = { posB, uvB, color, param1 };
  c = { posC, uvC, color, param1 };
}

RenderQuad renderTexturedRect(TexturePtr texture, Vec2F minPosition, float textureScale, Vec4B color, float param1) {
  return RenderQuad(std::move(texture), minPosition, textureScale, color, param1);
}

RenderQuad renderTexturedRect(TexturePtr texture, RectF const& screenCoords, Vec4B color, float param1) {
  return RenderQuad(std::move(texture), screenCoords, color, param1);
}

RenderQuad renderFlatRect(RectF const& rect, Vec4B color, float param1) {
  return RenderQuad(rect, color, param1);
}

RenderPoly renderFlatPoly(PolyF const& poly, Vec4B color, float param1) {
  return RenderPoly(poly.vertices(), color, param1);
}

}

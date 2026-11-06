module;
import std;
import star.format;
import star.json;
import star.string;
import star.json_extra;

// Match client include order for SIMD intrinsics used by xxhash.
import star.lua;
import star.lua_converters;
import star.client_application;
import star.image;
import star.bi_map;
import star.ref_ptr;
import star.renderer;

module star.rendering_lua_bindings;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeRenderingCallbacks(ClientApplication& app) {
  LuaCallbacks callbacks;
  auto frames = callbacks.registerCallbackWithSignature<unsigned>("framesSkipped", [&app] { return app.framesSkipped(); });
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  auto setGroup = callbacks.registerCallback("setPostProcessGroupEnabled", [&app](String const& group, bool enabled, std::optional<bool> save) {
    return app.setPostProcessGroupEnabled(group, enabled, save);
  });
  if (!setGroup)
    return std::unexpected(std::move(setGroup.error()));
  auto getGroup = callbacks.registerCallback("postProcessGroupEnabled", [&app](String const& group) {
    return app.postProcessGroupEnabled(group);
  });
  if (!getGroup)
    return std::unexpected(std::move(getGroup.error()));
  auto groups = callbacks.registerCallback("postProcessGroups", [&app] { return app.postProcessGroups(); });
  if (!groups)
    return std::unexpected(std::move(groups.error()));

  auto setParameter = callbacks.registerCallback("setEffectParameter", [&app](String const& effectName, String const& effectParameter, RenderEffectParameter const& value) -> std::expected<void, StarException> {
    return app.renderer()->setEffectScriptableParameter(effectName, effectParameter, value);
  });
  if (!setParameter)
    return std::unexpected(std::move(setParameter.error()));
  auto getParameter = callbacks.registerCallback("getEffectParameter", [&app](String const& effectName, String const& effectParameter) {
    return app.renderer()->getEffectScriptableParameter(effectName, effectParameter);
  });
  if (!getParameter)
    return std::unexpected(std::move(getParameter.error()));
  auto setPasses = callbacks.registerCallback("setPostProcessLayerPasses", [&app](String const& layer, unsigned passes) {
    return app.setPostProcessLayerPasses(layer, passes);
  });
  if (!setPasses)
    return std::unexpected(std::move(setPasses.error()));
  return callbacks;
}

}

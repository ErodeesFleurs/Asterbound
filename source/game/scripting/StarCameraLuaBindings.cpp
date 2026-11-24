module;
import star.json;
import star.vector;
import star.game_types;
import star.lua;
import star.lua_converters;
import star.ordered_map;
import star.rect;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.math_common;
import star.world_geometry;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.world_camera;

module star.camera_lua_bindings;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeCameraCallbacks(Root& root, WorldCamera& camera) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.register_callback_with_signature<Vec2F>(
        "position", [&camera]() { return camera.centerWorldPosition(); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks.register_callback_with_signature<float>("pixelRatio", [&camera]() { return camera.pixelRatio(); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setPixelRatio", [&root, &camera](float pixelRatio, std::optional<bool> smooth) -> std::expected<void, StarException> {
    // The direct setter changes the displayed transform before the next
    // configuration observation, so reject invalid zoom here as well.
    if (!is_finite(pixelRatio) || pixelRatio < 0.25f || pixelRatio > 1000000.f)
      return std::unexpected(StarException("camera pixel ratio must be finite and between 0.25 and 1000000"));
    if (smooth.value_or(false))
      camera.setTargetPixelRatio(pixelRatio);
    else
      camera.setPixelRatio(pixelRatio);
    auto configuration = root.configuration();
    if (!configuration)
      return std::unexpected(std::move(configuration.error()));
    return (*configuration)->set("zoomLevel", pixelRatio);
  }); !result)
    return std::unexpected(std::move(result.error()));

  if (auto result =
        callbacks.register_callback_with_signature<Vec2U>("screenSize", [&camera]() { return camera.screenSize(); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<RectF>("worldScreenRect",
                                                                   [&camera]() { return camera.worldScreenRect(); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks.register_callback_with_signature<RectI>("worldTileRect", [&camera]() { return camera.worldTileRect(); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks.register_callback_with_signature<Vec2F>("tileMinScreen", [&camera]() { return camera.tileMinScreen(); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<Vec2F, Vec2F>(
        "screenToWorld", [&camera](Vec2F position) { return camera.screenToWorld(position); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<Vec2F, Vec2F>(
        "worldToScreen", [&camera](Vec2F position) { return camera.worldToScreen(position); });
      !result)
    return std::unexpected(std::move(result.error()));

  return callbacks;
}

}

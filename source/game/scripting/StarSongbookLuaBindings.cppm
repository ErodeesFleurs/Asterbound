module;
import std;
import star.format;
import star.string;
import star.json;
import star.thread;
import star.vector;
// Match existing consumers' include order for SIMD intrinsics used by xxhash.
import star.game_types;

import star.lua;
import star.lua_converters;

import star.songbook;

export module star.songbook_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeSongbookCallbacks(Songbook& songbook);
}

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeSongbookCallbacks(Songbook& songbook) {
  LuaCallbacks callbacks;

  if (auto registration = callbacks.registerCallbackWithSignature<void, Json, String>(
        "play", [&songbook](Json const& song, String const& timeSource) { return songbook.play(song, timeSource); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, String, Vec2F>(
        "keepAlive",
        [&songbook](String const& instrument, Vec2F const& position) {
          return songbook.keepAlive(instrument, position);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<void>("stop", [&songbook]() { return songbook.stop(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<bool>("active", [&songbook]() { return songbook.active(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::optional<String>>(
        "band", [&songbook]() { return songbook.timeSource(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::optional<String>>(
        "instrument", [&songbook]() { return songbook.instrument(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "instrumentPlaying", [&songbook]() { return songbook.instrumentPlaying(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<Json>("song", [&songbook]() { return songbook.song(); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

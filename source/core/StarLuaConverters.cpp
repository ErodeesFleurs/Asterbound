import std;
import star.format;
import star.lua_converters;
import star.lua;
import star.color;
import star.string_view;

namespace Star {

std::expected<void, StarException> LuaConverter<Color>::from(LuaStack stack, Color const& c) {
  auto alpha = c.alpha();
  return alpha == 255
    ? stack.push_array(std::initializer_list<std::uint8_t>{c.red(), c.green(), c.blue()})
    : stack.push_array(std::initializer_list<std::uint8_t>{c.red(), c.green(), c.blue(), alpha});
}

std::expected<std::optional<Color>, StarException> LuaConverter<Color>::to(LuaStackValue v) {
  if (v.is<LuaTable>()) {
    std::array<int, 3> channels;
    for (std::size_t i = 0; i < channels.size(); ++i) {
      auto channel = v.maybe_get<int>(i + 1);
      if (!channel)
        return std::unexpected(std::move(channel.error()));
      if (!*channel)
        return std::nullopt;
      channels[i] = **channel;
    }
    Color c = Color::rgba(channels[0], channels[1], channels[2], 255);
    auto alpha = v.maybe_get<int>(4);
    if (!alpha)
      return std::unexpected(std::move(alpha.error()));
    if (*alpha)
      c.set_alpha(**alpha);
    return c;
  } else if (v.is<LuaString>()) {
    auto text = v.string_view().utf8();
    auto color = Color::create(StringView(text.substr(0, text.find('\0'))));
    if (!color) {
      if (color.error().is<ColorException>())
        return std::nullopt;
      return std::unexpected(std::move(color.error()));
    }
    return std::move(*color);
  }
  return std::nullopt;
}

std::expected<void, StarException> LuaConverter<LuaCallbacks>::from(LuaStack stack, LuaCallbacks const& callbacks) {
  auto const& lifetime = stack.engine().promise_lifetime();
  return lifetime.stop_possible() ? stack.push_callback_table(callbacks.with_lifetime(lifetime)) : stack.push_callback_table(callbacks);
}

std::expected<std::optional<LuaCallbacks>, StarException> LuaConverter<LuaCallbacks>::to(LuaStackValue) {
  return std::nullopt;
}
}

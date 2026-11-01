import std;
import star.format;
import star.lua_converters;
import star.lua;
import star.color;
import star.string_view;

namespace Star {

std::expected<void, StarException> LuaConverter<Color>::from(LuaStack stack, Color const& c) {
  return c.alpha() == 255
    ? stack.pushArray(std::initializer_list<std::uint8_t>{c.red(), c.green(), c.blue()})
    : stack.pushArray(std::initializer_list<std::uint8_t>{c.red(), c.green(), c.blue(), c.alpha()});
}

std::expected<std::optional<Color>, StarException> LuaConverter<Color>::to(LuaStackValue v) {
  if (v.is<LuaTable>()) {
    std::array<int, 3> channels;
    for (std::size_t i = 0; i < channels.size(); ++i) {
      auto channel = v.maybeGet<int>(i + 1);
      if (!channel)
        return std::unexpected(std::move(channel.error()));
      if (!*channel)
        return std::nullopt;
      channels[i] = **channel;
    }
    Color c = Color::rgba(channels[0], channels[1], channels[2], 255);
    auto alpha = v.maybeGet<int>(4);
    if (!alpha)
      return std::unexpected(std::move(alpha.error()));
    if (*alpha)
      c.setAlpha(**alpha);
    return c;
  } else if (v.is<LuaString>()) {
    auto text = v.stringView().utf8();
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

std::expected<void, StarException> LuaConverter<LuaCallbacks>::from(LuaStack stack, LuaCallbacks const& c) {
  auto created = stack.createTable(0, c.callbacks().size());
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto table = std::move(*created);
  for (auto& callback : c.callbacks()) {
    auto function = stack.engine().createWrappedFunction(callback.second);
    if (!function)
      return std::unexpected(std::move(function.error()));
    auto result = table.set(callback.first, std::move(*function));
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<std::optional<LuaCallbacks>, StarException> LuaConverter<LuaCallbacks>::to(LuaStackValue) {
  return std::nullopt;
}
}

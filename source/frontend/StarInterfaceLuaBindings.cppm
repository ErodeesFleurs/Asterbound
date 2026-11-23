module;
import std;
import star.format;

namespace Star {
class LuaCallbacks;
class MainInterface;
class UniverseClient;
}

export module star.interface_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeInterfaceCallbacks(MainInterface& mainInterface);
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_chat_callbacks(MainInterface& main_interface, UniverseClient& client);
}

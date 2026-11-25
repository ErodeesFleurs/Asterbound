module;
import std;
import star.format;
import star.lua;

namespace Star {
class LuaCallbacks;
class Item;
}

export module star.item_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_item_callbacks(Item& item);
  // The slot must outlive the callbacks and hold an item when invoked.
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_item_slot_callbacks(
      std::reference_wrapper<std::shared_ptr<Item> const> item_slot);
}


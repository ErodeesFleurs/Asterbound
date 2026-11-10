# Unsorted

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

These are functions that aren't in any specific table.

---

#### `LuaFunction` loadstring(`String` source, [`String` name, [`LuaTable` env]])

Compiles the provided **source** and returns it as a callable function.
Syntax errors raise an error; use `pcall` to catch compilation failures.

- **name** is used for error messages, the default is `loadstring`.
- **env** is used as the environment of the returned function, the default is `_ENV`.

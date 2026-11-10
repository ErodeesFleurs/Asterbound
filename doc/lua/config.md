# config

[Lua API](README.md#base-api) · [Modding guide](../modding.md)

The `config` Lua bindings provide access to configuration parameters.

---

#### `Json` config.getParameter(`String` parameter, `Json` default)

Returns the value for the specified config parameter. If there is no value set, returns the default.

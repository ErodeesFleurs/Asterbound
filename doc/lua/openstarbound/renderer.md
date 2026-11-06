# Renderer

The new `renderer` table is accessible from almost every clientside script and allows configuring shaders.

Calls belong to the client application owner. Graphics-state reads and scriptable parameter writes use an ordered main-thread barrier and may wait for pending presentation. Getters observe preceding writes. A scriptable parameter type mismatch raises an error in the setter itself: `pcall` can catch it without invalidating later frames.

---

#### `void` renderer.setPostProcessGroupEnabled(`String` group, `bool` enabled, [`bool` save])

Enables or disables a post process shader group. If save is true, this change is saved to configuration as well.

---

#### `bool` renderer.postProcessGroupEnabled(`String` group)

Returns true if the specified post process group is enabled.

---

#### `Json` renderer.postProcessGroups()

Returns every post process group. Identical to grabbing them from client.config with root.assetJson.

---

#### `void` renderer.setEffectParameter(`String` effectName, `String` parameterName, `Json` value)

Sets the specified scriptable parameter of the specified shader effect to the provided value. 
This is accessed from the shader as a uniform and must be defined in the effect's configuration.

The setter resolves the declared type, converts numeric Lua values for `int` parameters (truncating fractions toward zero), and applies the value in one ordered main-thread transaction. Other type mismatches raise immediately and leave the previous value unchanged. Missing effects or parameters remain no-ops; their getters return `nil`.

---

#### `Json` renderer.getEffectParameter(`String` effectName, `String` parameterName)

Returns the specified scriptable parameter of the specified shader effect.

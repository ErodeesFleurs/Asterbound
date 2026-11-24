# Input

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

The `input` table allows access to user input from most client-side Lua scripts.

---

#### `Maybe<unsigned>` input.bindDown(`String` categoryId, `String` bindId)

If this bind was pressed this frame, returns how many times it has been pressed. Returns `nil` otherwise.

---

#### `bool` input.bindHeld(`String` categoryId, `String` bindId)

An action stays held until the last physical input that activated it is released. Two controllers pressing the same named button are distinct inputs.

---

#### `Maybe<unsigned>` input.bindUp(`String` categoryId, `String` bindId)

If this bind was released this frame, returns how many times it has been released. Returns `nil` otherwise.

---

#### `Maybe<unsigned>` input.keyDown(`String` keyName, [`StringList` modNames])

Returns the number of presses this logic tick, including repeats, or `nil` if none match. When `modNames` is supplied, each press is matched against its own modifier snapshot; omit the argument to count all presses.

---

#### `bool` input.keyHeld(`String` keyName)

Returns whether the key is held.

---

#### `bool` input.key(`String` keyName)

Same as `input.keyHeld`.

---

#### `Maybe<unsigned>` input.keyUp(`String` keyName)

Returns the number of releases this logic tick, or `nil` if there were none.

---

#### `Maybe<List<Vec2F>>` input.mouseDown(`String` buttonName)

Returns the positions of presses of this button in the current logic tick, in event order, or `nil`.

---

#### `bool` input.mouseHeld(`String` buttonName)

Returns true if the specified mouse button is held.

---

#### `bool` input.mouse(`String` buttonName)

Same as `input.mouseHeld`.

---

#### `Maybe<List<Vec2F>>` input.mouseUp(`String` buttonName)

Returns the positions of releases of this button in the current logic tick, in event order, or `nil`.

---

#### `void` input.resetBinds(`String` categoryId, `String` bindId)

Resets this bind to its default keys.

---

#### `void` input.setBinds(`String` categoryId, `String` bindId, `Json` binds)

Sets the inputs for this `.binds` action. An empty array clears the action, including after restart. Use `input.resetBinds` to restore its defaults. These functions do not modify game actions in `bindings`.

---

#### `Json` input.getDefaultBinds(`String` categoryId, `String` bindId)

Returns the default keys for this bind.

---

#### `Json` input.getBinds(`String` categoryId, `String` bindId)

Returns the keys for this bind.

---

#### `Json` input.events()

Returns all input events for this frame.

---

#### `Vec2F` input.mousePosition()

Returns the mouse position in pixels relative to the bottom left of the screen.

---

#### `unsigned` input.getTag(`String` tagName)

Returns the amount of binds currently held with the given tag.

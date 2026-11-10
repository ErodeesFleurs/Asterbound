# Effect

[Lua API](../README.md#openstarbound-compatible-extensions) · [Base API](../statuseffect.md) · [Modding guide](../../modding.md)

<a id="the-effect-table-contains-bindings-specific-to-effect-which-are-available-in-addition-to-their-common-tables"></a>

The `effect` table contains additional bindings specific to status effects.

---

#### `String` effect.name()

Returns the effect asset name associated with the unique effect instance.

---
#### `void` effect.setDuration(`float` duration)

Sets the remaining duration of the effect to **duration** seconds when the duration is tracked.

---
#### `void` effect.setToolUsageSuppressed(`bool` suppressed)

Toggles whether this effect suppresses tool usage for the entity it is applied to.

---

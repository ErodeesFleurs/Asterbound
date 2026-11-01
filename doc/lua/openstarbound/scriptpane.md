# Script Pane

OpenStarbound extends the vanilla `pane` bindings described in `doc/lua/scriptpane.md` with additional helpers for interacting
with scripted panes at runtime.

---

#### `LuaCallbacks` pane.toWidget()

Returns the root widget of the pane as a table of widget callbacks, equivalent to calling `widget` helpers on the pane itself.
This is especially useful when a script pane needs to call functions such as `widget.getChildAt` on its root.

---

#### `Vec2I` pane.getPosition()

Returns the pane's current on-screen position.

---

#### `void` pane.setPosition(`Vec2I` position)

Moves the pane to the specified screen position.

---

#### `Vec2I` pane.getSize()

Returns the pane's current size in pixels.

---

#### `void` pane.setSize(`Vec2I` size)

Resizes the pane to the supplied dimensions.

---

#### `void` pane.setScissoringEnabled(`bool` enabled)

Enables or disables clipping to the pane's own bounds. Ancestor/window clipping still applies; child widgets retain their own scissoring policy. Use the corresponding `widget.setScissoringEnabled` for a child. This does not change root size, visibility or input routing.

---

#### `void` pane.setBackground(`String` header, [`String` body], [`String` footer])

Replaces the native pane background images. Omitted body/footer default to
empty strings; empty strings clear those pieces. Root size becomes the maximum
piece width and sum of piece heights, using actual image sizes. Child geometry,
anchors and offsets are not rewritten; native image-load errors propagate.

#### `float` pane.scale()

Returns the interface scale currently applied to the pane. This mirrors `interface.scale()` but is available directly on the pane
object.

---

#### `bool` pane.isDisplayed()

Returns `true` if the pane is currently being displayed.

---

#### `bool` pane.hasFocus()

Returns `true` if the pane currently has keyboard focus.

---

#### `String` pane.anchor()

Returns the pane's current anchor as a string.

---

#### `void` pane.setAnchor(`String` anchor)

Sets the pane's anchor to the specified value.

---

#### `Vec2I` pane.anchorOffset()

Returns the pane's current anchor offset.

---

#### `void` pane.setAnchorOffset(`Vec2I` offset)

Sets the pane's anchor offset to the specified value.

---

#### `Vec2I` pane.getScreenPosition()

Returns the pane's current position on the screen, taking into account its anchor and offset.

---

#### `void` pane.setScreenPosition(`Vec2I` position)

Moves the pane to the specified screen position, automatically calculating the appropriate position, anchor, and offset values.

---

#### `void` pane.show()

Shows the pane if it is hidden.

---

#### `void` pane.hide()

Hides the pane if it is currently shown.

---

#### `LuaCallbacks` pane.addWidget(`Json` config, [`String` widgetName])

In vanilla Starbound this function returns nothing. OpenStarbound now returns widget callbacks for the newly created widget,
allowing scripts to immediately operate on it without a separate `widget` lookup.

---

#### `bool` pane.removeWidget(`String` widgetName)

`pane.removeWidget` now returns whether the named widget existed and was removed.

## Shared script pane runtime

Script panes have the standard `storage` table. Its JSON-compatible contents
survive script reinitialization, asset reload, and replacement of the underlying
Lua engine while the pane instance exists. `uninit()` runs before storage is
captured. No explicit checkpoint call is required; modify `storage` directly.
Use `jarray()` when an empty array must remain an array across a JSON round trip.
Storage belongs to the instance, not global configuration or a player save.
The Lua interface manager decides when an instance is discarded.

Keep derived caches and runtime handles in locals; checkpoint only data needed
across reinitialization. Retained native widgets already own their text and
selection, so do not mirror those values into `storage`. Share small pure helpers
through explicit namespaces without allocating runtime resources at module load.
Use the existing interface manager event queue and lifecycle hooks rather than
adding a second subscription or timer layer.

`pane.isVisible()` queries drawing visibility. `pane.show()` and `pane.hide()`
change drawing visibility independently of display membership, reported by
`pane.isDisplayed()`. PaneManager continues ticking hidden displayed panes.

`render()` is an optional hook called immediately before drawing child widgets.
Canvas commands produced there are drawn in the same frame and persist until
cleared. The usual `init`, `update(dt)`, `uninit`, `displayed`, and `dismissed`
hooks remain available. Define functions at script top level and use bindings in
`init`, after callback tables have been installed.

`dismissed()` is a true presentation-removal hook. Asset/engine reload and
centralized capability replacement run `uninit()` without issuing a false
`dismissed()` or removing surviving panes from the native display stack.
Scripts are explicitly resumed after rebinding; closed retained windows stay
dormant until reopened. Put cancellation/commit of a presentation request in
`dismissed()`, not in the general `uninit()` hook.

A pane configuration can specify top-level `size: [width, height]` in interface
pixels. Otherwise initial root bounds include declarative children and any
native background. When a script changes Canvas or layout dimensions, update
the root with `pane.setSize(size)` to keep drawing bounds and input geometry in
sync.
Anchored composite HUDs may deliberately keep an explicit zero-sized root and
negative child positions. Disable root scissoring for their out-of-bounds
drawing and hit-test the actual child geometry rather than inventing root bounds.

Canvas configurations may set `ignoreInterfaceScale: true` to use physical
pixel drawing coordinates. This forwards the existing Canvas scale mode;
pane and widget bounds still use interface pixels and must be sized accordingly.
Top-level `interactive: false` keeps a displayed pane rendering and ticking
while excluding it from input dispatch, capture and hit testing.
An interactive persistent HUD can set `dismissable: false` to disable the
default GuiClose dismissal without consuming Esc or disabling pointer input.

For Canvas tooltips, `createTooltip(screenPosition)` can return a label string,
a GUI JSON definition, or `nil`. Its argument is a screen position in interface
pixels, not physical window pixels. Subtract `pane.getScreenPosition()` before
comparing it with Canvas-local geometry. The hook does not accept opaque
native pane userdata.

A non-nil custom `createTooltip` result has priority. Otherwise the optional
`tooltipItem(screenPosition)` hook can return an engine-produced opaque Item
userdata for the native tooltip builder. Its position is in interface pixels,
like `createTooltip`. A **defined hook returning nil suppresses** automatic
ItemGrid/ItemSlot lookup; a missing hook preserves that fallback. Descriptors,
arbitrary userdata and opaque native Pane handles are not valid Item results.

Client UI panes consult the centralized `interface.itemTooltip` service, which
uses the current UI player for armor and other previewable items. Standalone
panes without that service keep the native no-viewer fallback. Both hooks use
the normal guarded script lifecycle; the host stores no window-specific player
or tooltip factory.

### Consuming input

The optional `onInput(event)` hook runs before normal input routing while the
pane is visible. Hidden panes reject input before invoking the hook or routing
to focused widgets; global open/toggle actions belong to the Lua interface
manager's controller, not hidden instances. Return `true` to consume the event
or `false` to let ordinary GuiClose handling and focused-widget routing run.
Missing hooks do not consume input. This lets Lua handle submit/cancel actions
while keeping real TextBox editing, keyboard capture, selection, and
committed-text IME input.

Every event has an `actions` array containing the engine's interface-action
names for that event, without a pane-specific semantic mapping. Key releases
have no resolved actions; scripts that require press/release pairing retain the
press's action names in Lua. Input events contain:

* `type`: `keyDown`, `keyUp`, `textInput`, `move`, `buttonDown`, `buttonUp`,
  `wheel`, or `controller`.
* `key` for key events, numeric `mods` for key presses, and `text` for committed
  text.
* Pane-local `position` and interface-pixel `screenPosition` for pointer events.
* `physicalPosition` for `move`, `buttonDown` and `buttonUp`: the actual raw
  event's floating-point physical window-pixel coordinates. It is not rebuilt
  from the rounded/scaled interface coordinates. Wheel/nonpointer events do
  not have this field; existing `position`/`screenPosition` semantics are unchanged.
* Zero-based `button` for mouse-button events or `direction` (`1` up, `-1` down)
  for wheel events.

```lua
function init()
  storage.enabled = storage.enabled ~= false
end

function onInput(event)
  for _, action in ipairs(event.actions) do
    if event.type == "keyDown" and action == "InterfaceShowHelp" then
      storage.enabled = not storage.enabled
      return true
    end
  end
  return false
end
```

### Native integration

Module `star.base_script_pane` exports the generic `BaseScriptPane` host.
`create(context, config)` constructs its widgets without initializing scripts.
Install service tables using `addCallbacks(name, callbacks)` and choose the
existing shared engine with `setLuaRoot(root)` before `displayed()` (normally
called by PaneManager). `dismissed()` invokes dismissal and uninitialization
hooks. It must finish while all borrowed service targets are still alive.
Component-owned callback group names take precedence over inherited LuaRoot
tables, including on reinitialization and when added to a live context. Scripts
must not define replacement tables under those reserved group names. An explicit
empty callback group shadows an inherited service without exposing its old
functions; groups not owned by the component remain inherited. Every
component-owned callback installed in a context carries that context's lifetime
lease, including callbacks added while it is live. Shutdown hooks run before
the lease retires; error cleanup also retires it. Escaped functions from an old
context remain rejected after reinitialization or rebinding, even if the old
service target is still alive. Service owners must keep borrowed targets alive
through shutdown.

`removeCallbacks(name)` removes a service table. To rebind services, centrally
stop the instance, replace callback groups/root, then display it again.
`LuaRoot::generation()` identifies successful engine replacements even when the
root pointer is unchanged. `LuaBaseComponent` handles generation and asset reload
at top-level script entry boundaries. Synchronous native callbacks may reenter
the same component; automatic reload stays pending until the active Lua call
returns, rather than retiring its callbacks mid-execution. Panes do not maintain
a second reload protocol.

`call(method, JsonArray)` spreads JSON arguments positionally and returns
`expected<optional<Json>, StarException>`; missing methods and uninitialized
scripts return an empty optional. `hasMethod(method)` checks for a function
without calling it. `inputJson(event)` exposes the generic event representation,
including coordinates relative to that pane. `forwardEvent(event)` directly
uses Pane's native widget routing without invoking `onInput` or the script
pane's default GuiClose policy; an external Lua controller can therefore own
input routing without duplicating TextBox behavior.

Registration, opening and closing, session/world scope, ordering, input policy,
semantic action mapping, and event queues belong to the Lua interface manager,
not pane configuration parsing or native per-pane policy fields. Deliver
manager events by invoking the selected script method through `call`.
Keep displayed instances in PaneManager for native focus, layers, geometry,
rendering, and child-widget updates; do not tick them a second time.

## Lua-owned instance registration

`/interface.config:panes` is read by the client-side Lua manager:

```json
{
  "example": {
    "config": "/interface/example/example.config",
    "layer": "Hud",
    "scope": "session",
    "open": true,
    "order": 20,
    "actions": {"InterfaceShowHelp": "toggle"},
    "events": {"refresh": "refresh"}
  }
}
```

`config` is an asset path or resolved pane JSON. `scope` is `"session"`,
`"player"` or `"world"`. Player instances require a player and survive world
changes, but are released when the player changes or becomes unavailable.
World instances open only in-world and are released on world or player changes.
`open` controls initial/automatic opening. `order` controls initial creation
order; native layers, capture, modal barriers and current stacking determine
input priority. A normal window uses the same runtime with `"layer": "Window"`.
Use existing `gui.panefeature` anchor/offset declarations so initial native
placement is correct before script `init`.

`retain: true` keeps an ordinarily dismissed instance for later reopening,
preserving its native widgets and script storage. This is Lua manager policy,
not a native pane flag. Explicit `ui.close`, registration replacement, scope
expiry and interface shutdown destroy the instance, including one that is
currently closed but retained.
Toggling a displayed window uses ordinary dismissal and honors `retain`.

Action mappings call `onAction(name, pressed, event)` and consume input only
when the handler returns true. The manager preserves press/release pairing
through controller reload; closing an instance cancels its outstanding
presses with `event.cancelled = true`. Native TextBox routing remains available
for unconsumed editing events. Event mappings invoke the named function with
one payload; unmapped events invoke `onEvent(name, payload)`.

`globalActions` maps engine action names to manager operations `"open"` or
`"toggle"`, for example `{"InterfaceCodex": "toggle"}`. Unlike local `actions`,
it can create an unopened instance and does not invoke `onAction`. Native
keyboard capture and modal windows take precedence. Held-key repeats do not
reapply the operation; press/release state survives controller reload.

The public helpers in `/interface/opensb/ui/api.lua` provide
`ui.register(id, definition)`, `ui.open(id)`, `ui.toggle(id)`, `ui.close(id)`, and
`ui.send(id, event, payload)`. Requests are queued until the calling script
returns. Events posted by a handler wait for a later delivery boundary, rather
than recursively mutating interface state. Native registered IDs cannot be
replaced by Lua registrations.

### Client metadata service

`client.storage(namespace)` returns callbacks `get(fallback?)` and `set(value)`
for one PlayerStorage metadata namespace. It is separate from instance
`storage`: metadata persists across instance destruction and application
restart. Names must be nonempty; `"order"` is reserved. Values must be
JSON-compatible. This service does not expose arbitrary paths, files, or root
configuration. For example, chat uses `"chatHistory"` to retain its existing
input-history storage; new consumers should choose unique mod-prefixed names.

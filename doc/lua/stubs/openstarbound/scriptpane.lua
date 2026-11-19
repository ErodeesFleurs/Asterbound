---@meta

--- pane API

--- Returns the root widget of the pane as a table of widget callbacks, equivalent to calling `widget` helpers on the pane itself. This is especially useful when a script pane needs to call functions such as `widget.getChildAt` on its root. ---
---@return LuaCallbacks
function pane.toWidget() end

--- Returns the pane's current on-screen position. ---
---@return Vec2I
function pane.getPosition() end

--- Moves the pane to the specified screen position. ---
---@param position Vec2I
function pane.setPosition(position) end

--- Returns the pane's current size in pixels. ---
---@return Vec2I
function pane.getSize() end

--- Resizes the pane to the supplied dimensions. ---
---@param size Vec2I
function pane.setSize(size) end

--- Controls clipping to the pane's own bounds; ancestor/window and child clipping still apply.
---@param enabled boolean
function pane.setScissoringEnabled(enabled) end

--- Native background replacement; root size is max width/summed heights. Empty strings clear.
---@param header string
---@param body? string
---@param footer? string
function pane.setBackground(header, body, footer) end

--- Returns the interface scale currently applied to the pane. This mirrors `interface.scale()` but is available directly on the pane object. ---
---@return number
function pane.scale() end

--- Returns `true` if the pane is currently being displayed. ---
---@return boolean
function pane.isDisplayed() end

--- Returns `true` if the pane currently has keyboard focus. ---
---@return boolean
function pane.hasFocus() end

--- Returns the pane's current anchor as a string. ---
---@return string
function pane.getAnchor() end

--- Sets the pane's anchor to the specified value.
function pane.setAnchor(anchor) end

--- Returns the pane's current anchor offset. ---
---@return Vec2I
function pane.getAnchorOffset() end

--- Sets the pane's anchor offset to the specified value. ---
---@param offset Vec2I
function pane.setAnchorOffset(offset) end

--- Returns the pane's current position on the screen, taking into account its anchor and offset. ---
---@return Vec2I
function pane.getScreenPosition() end

--- Moves the pane to the specified screen position, automatically calculating the appropriate position, anchor, and offset values. ---
---@param position Vec2I
function pane.moveToScreenPosition(position) end

--- Shows the pane if it is hidden. ---
function pane.show() end

--- Hides the pane if it is currently shown. ---
function pane.hide() end

--- Returns drawing visibility independently of display lifecycle membership. ---
---@return boolean
function pane.isVisible() end

--- In vanilla Starbound this function returns nothing. OpenStarbound now returns widget callbacks for the newly created widget, allowing scripts to immediately operate on it without a separate `widget` lookup. ---
---@param config Json
---@param widgetName string
---@return LuaCallbacks
function pane.addWidget(config, widgetName) end

--- `pane.removeWidget` now returns whether the named widget existed and was removed.
---@param widgetName string
---@return boolean
function pane.removeWidget(widgetName) end

---@class ScriptPaneInputEvent
---@field type string keyDown/keyUp/textInput/move/buttonDown/buttonUp/wheel/controller.
---@field actions string[]
---@field position? Vec2I Pane-local interface pixels for pointer events.
---@field screenPosition? Vec2I Screen interface pixels for pointer events.
---@field physicalPosition? Vec2F Actual raw physical pixels for move/buttonDown/buttonUp only.
---@field button? integer Zero-based mouse button.
---@field buttonName? string Canonical mouse button name on buttonDown.
---@field controllerButton? string Canonical controller button name on press.
---@field controller? integer Controller ID on button press.
---@field direction? integer Wheel: 1 up, -1 down.
---@field key? string
---@field mods? integer
---@field text? string

--- Optional script hook; true consumes before normal native input routing.
---@param event ScriptPaneInputEvent
---@return boolean
function onInput(event) end

--- Optional custom tooltip hook; non-nil string/GUI JSON takes priority over tooltipItem/native fallback.
---@param screenPosition Vec2I Interface pixels, not physical pixels.
---@return string|Json?
function createTooltip(screenPosition) end

--- Optional Item tooltip hook: missing preserves native ItemGrid/Slot lookup; defined nil suppresses it.
--- Actual opaque Item only, never descriptor JSON or Pane userdata. Uses current UI player preview service.
---@param screenPosition Vec2I Interface pixels.
---@return NativeItemHandle?
function tooltipItem(screenPosition) end

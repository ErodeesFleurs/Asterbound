# widget

The `widget` table contains functions to manipulate and get data about widgets in a scriptpane.

The widgetName passed into most of these functions can contain period separators for getting children.

Example:
```
widget.getPosition("itemScrollArea.itemList.1.name")
```

## General callbacks

These callbacks are available for all widgets.

---

#### `void` widget.playSound(`String` audio, [`int` loops = 0], [`float` volume = 1.0f])

Plays a sound.

---

#### `Vec2I` widget.getPosition(`String` widgetName)

Returns the position of a widget.

---

#### `void` widget.setPosition(`String` widgetName, `Vec2I` position)

Sets the position of a widget.

---

#### `Vec2I` widget.getSize(`String` widgetName)

Returns the size of a widget.

---

#### `void` widget.setSize(`String` widgetName, `Vec2I` size)

Sets the size of a widget.

---

#### `void` widget.setVisible(`String` widgetName, `bool` visible)

Sets the visibility of a widget.

---

#### `void` widget.active(`String` widgetName)

Returns whether the widget is visible.

---

#### `void` widget.focus(`String` widgetName)

Sets focus on the specified widget.

---

#### `void` widget.hasFocus(`String` widgetName)

Returns whether the specified widget is currently focused.

---

#### `void` widget.blur(`String` widgetName)

Unsets focus on the specified focused widget.

---

#### `Json` widget.getData(`String` widgetName)

Returns the arbitrary data value set for the widget.

---

#### `void` widget.setData(`String` widgetName, `Json` data)

Sets arbitrary data for the widget.
---

#### `String` widget.getChildAt(`Vec2I` screenPosition)

Returns the full name for any widget at screenPosition.

---

#### `bool` widget.inMember(`String` widgetName, `Vec2I` screenPosition)

Returns whether the widget contains the specified screenPosition.

---

#### `void` widget.addChild(`String` widgetName, `Json` childConfig, [`String` childName])

Creates a new child widget with the specified config and adds it to the specified widget, optionally with the specified name.

---

#### `void` widget.removeAllChildren(`String` widgetName)

Removes all child widgets of the specified widget.

---

#### `void` widget.removeChild(`String` widgetName, `String` childName)

Removes the specified child widget from the specified widget.

---

## Widget specific callbacks

These callbacks only work for some widget types.

---

#### `String` widget.getText(`String` widgetName)

Returns the text set in a TextBoxWidget.

---

#### `void` widget.setText(`String` widgetName, `String` text)

Sets the text of: LabelWidget, ButtonWidget, TextBoxWidget

---

#### `void` widget.setFontColor(`String` widgetName, `Color` color)

Sets the font color of: LabelWidget, ButtonWidget, TextBoxWidget

---

#### `void` widget.setImage(`String` widgetName, `String` imagePath)

Sets the image of an ImageWidget.

---

#### `void` widget.setImageScale(`String` widgetName, `float` imageScale)

Sets the scale of an ImageWidget.

---

#### `void` widget.setImageRotation(`String` widgetName, `float` imageRotation)

Sets the rotation of an ImageWidget.

---

#### `void` widget.setButtonEnabled(`String` widgetName, `bool` enabled)

Sets whether the ButtonWidget should be enabled.

---

#### `void` widget.setButtonImage(`String` widgetName, `String` baseImage)

Sets the baseImage of a ButtonWidget.

---

#### `void` widget.setButtonImages(`String` widgetName, `Json` imageSet)

Sets the full image set of a ButtonWidget.

```
{
  base = "image.png",
  hover = "image.png",
  pressed = "image.png",
  disabled = "image.png",
}
```

---

#### `void` widget.setButtonCheckedImages(`String` widgetName, `Json` imageSet)

Similar to widget.setButtonImages, but sets the images used for the checked state of a checkable ButtonWidget.

---

#### `void` widget.setButtonOverlayImage(`String` widgetName, `String` overlayImage)

Sets the overlay image of a ButtonWidget.

---

#### `bool` widget.getChecked(`String` widgetName)

Returns whether the ButtonWidget is checked.

---

#### `void` widget.setChecked(`String` widgetName, `bool` checked)

Sets whether a ButtonWidget is checked

---

#### `int` widget.getSelectedOption(`String` widgetName)

Returns the index of the selected option in a ButtonGroupWidget.

---

#### `int` widget.getSelectedData(`String` widgetName)

Returns the data of the selected option in a ButtonGroupWidget. Nil if no option is selected.

---

#### `void` widget.setSelectedOption(`String` widgetName, `int` index)

Sets the selected option index of a ButtonGroupWidget.

---

#### `void` widget.setOptionEnabled(`String` widgetName, `int` index, `bool` enabled)

Sets whether a ButtonGroupWidget option should be enabled.

---

#### `void` widget.setOptionVisible(`String` widgetName, `int` index, `bool`, visible)

Sets whether a ButtonGroupWidget option should be visible.

---

#### `void` widget.setProgress(`String` widgetName, `float` value)

Sets the progress of a ProgressWidget. Value should be between 0.0 and 1.0.

---

#### `void` widget.setSliderEnabled(`String` widgetName, `bool` enabled)

Sets whether the SliderBarWidget should be enabled.

---

#### `float` widget.getSliderValue(`String` widgetName)

Gets the current value of a SliderBarWidget.

---

#### `void` widget.setSliderValue(`String` widgetName, `int` newValue)

Sets the current value of a SliderBarWidget.

---

#### `void` widget.getSliderRange(`String` widgetName, `int` newMin, `int` newMax, [`int` newDelta])

Sets the minimum, maximum and (optionally) delta values of a SliderBarWidget.

---

#### `void` widget.clearListItems(`String` widgetName)

Clears all items in a ListWidget.

---

#### `String` widget.addListItem(`String` widgetName)

Adds a list item to a ListWidget using the configured template, and returns the name of the added list item.

---

#### `void` widget.removeListItem(`String` widgetName, `size_t` at)

Removes a list item from a ListWidget at a specific index.

---

#### `String` widget.getListSelected(`String` widgetName)

Returns the name of the currently selected widget in a ListWidget.

---

#### `void` widget.setListSelected(`String` widgetName, `String` selected)

Sets the selected widget of a ListWidget.

---

#### `void` widget.registerMemberCallback(`String` widgetName, `String` callbackName, `LuaFunction` callback)

Registers a member callback for a ListWidget's list items to use. Removing the issuing callback group or closing/reloading the script releases the retained Lua function; existing native rows become inert. Register again for new rows in the replacement session.

---

#### `ItemBag` widget.itemGridItems(`String` widgetName)

Returns the full item bag contents of an ItemGridWidget.

---

#### `ItemDescriptor` widget.itemSlotItem(`String` widgetName)

Returns the descriptor of the item in the specified item slot widget.

---

#### `void` widget.setItemSlotItem(`String` widgetName, `Json` itemDescriptor)

Sets the item in the specified item slot widget.

---

#### `void` widget.setItemSlotProgress(`String` widgetName, `float` progress)

Sets the progress overlay on the item slot to the specified value (between 0 and 1).

---

#### `CanvasWidget` widget.bindCanvas(`String` widgetName)

Binds the canvas widget with the specified name as userdata for easy access. The `CanvasWidget` has the following methods:

The userdata retains the native canvas surface, but script access retires with its issuing binding group/session or destroyed widget owner. An `interface.bindCanvas` handle also requires its interface host to remain attached. Rebind after reopening or reloading a pane; retaining an old handle does not revive access. See [binding lifetimes](openstarbound/utility.md#binding-lifetimes).

With `ignoreInterfaceScale`, drawing and clip coordinates are physical pixels,
but rendering still adds the raw, **unscaled** Widget screen position. Widget
layout, hit testing and ancestor clipping retain their interface-coordinate
semantics. For a canvas at pane-local `{0, 0}` drawing absolute window-pixel
geometry, subtract `pane.getScreenPosition()` directly; do not multiply that
offset by `interface.scale()`.

##### `Vec2I` size()

Returns the size of the canvas.

##### `void` clear()

Clears the canvas.

##### `void` pushClipRect(`RectF` rect)
##### `void` popClipRect()

Queues a Canvas-local clip rectangle, intersecting any current Canvas clip.
Pop restores the preceding clip; popping an empty stack raises an error.
Ancestor/native clipping still applies and is restored after Canvas rendering.
Clips use normal Canvas units (physical pixels with `ignoreInterfaceScale`).
`clear()` clears queued drawing and the clip stack. These are drawing commands,
not changes to widget geometry or input hit testing.

##### `void` drawDrawable(`Drawable` drawable, [`Vec2F` position], [`float` scale], [`Vec2F` translation], [`float` finalScale])
##### `void` drawDrawables(`List<Drawable>` drawables, [`Vec2F` position], [`float` scale], [`Vec2F` translation], [`float` finalScale])

Queues native Drawable geometry at Canvas-local `position` (default `{0, 0}`).
Geometry is transformed in order: **translate by `translation`, scale by
`scale`, then scale by `finalScale`**. Defaults are zero translation and unit
scales. The two native float scaling steps are separate; do not combine them
when exact float operation boundaries matter. Position is the final drawing
origin, not the pre-scale translation.

##### `void` drawJsonDrawable(`Json` drawable, [`Vec2F` position])
##### `void` drawJsonDrawables(`JsonArray` drawables, [`Vec2F` position])

Parses native Drawable JSON definitions and queues them at the optional
Canvas-local position. These JSON variants do not accept the extra transform
arguments above; asset/Drawable parsing errors propagate.


##### `Vec2I` mousePosition()

Returns the mouse position relative to the canvas.

##### `void` drawImage(`String` image, `Vec2F` position, [`float` scale], [`Color` color], [`bool` centered])

Draws an image to the canvas.

##### `void` drawImageDrawable(`String` image, `Vec2F` position, [`Variant<Vec2F, float>` scale], [`Color` color], [`float` rotation])

Draws an image to the canvas, centered on position, with slightly different options.

##### `void` drawImageRect(`String` texName, `RectF` texCoords, `RectF` screenCoords, [`Color` color])

Draws a rect section of a texture to a rect section of the canvas.

##### `void` drawTiledImage(`String` image, `Vec2F` offset, `RectF` screenCoords, [`float` scale], [`Color` color])

Draws an image tiled (and wrapping) within the specified screen area.

##### `void` drawLine(`Vec2F` start, `Vec2F` end, [`Color` color], [`float` lineWidth])

Draws a line on the canvas.

##### `void` drawRect(`RectF` rect, `Color` color)

Draws a filled rectangle on the canvas.

##### `void` drawPoly(`PolyF` poly, `Color` color, [`float` lineWidth])

Draws a polygon on the canvas.

##### `void` drawTriangles(`List<PolyF>` triangles, [`Color` color])

Draws a list of filled triangles to the canvas.

##### `void` drawText(`String` text, `Json` textPositioning, `unsigned` fontSize, [`Color` color], [`float` lineSpacing], [`String` font], [`String` directives])

Draws text on the canvas. textPositioning is in the format:

```lua
{
  position = {0, 0},
  horizontalAnchor = "left", -- left, mid, right
  verticalAnchor = "top", -- top, mid, bottom
  wrapWidth = nil, -- wrap width in Canvas units or nil
  charLimit = nil -- rendered codepoint limit, excluding rich-text commands
}
```

##### `void` drawText(`String` text, `Json` textPositioning, `Json` textStyle)

The style-table form accepts the complete engine TextStyle: `fontSize`, `font`, `color`, `shadow`, `lineSpacing`, `directives` and `backDirectives`. Unspecified fields use engine TextStyle defaults. The numeric-size signature above is unchanged.

##### `RectF` measureText(`String` text, `Json` textPositioning, `Json` textStyle)
##### `RectF` measureText(`String` text, `Json` textPositioning, `unsigned` fontSize, [`Color` color], [`float` lineSpacing], [`String` font], [`String` directives])

Returns `{xMin, yMin, xMax, yMax}` layout bounds in Canvas-local units without drawing or queuing an operation. Uses the same engine layout, rich-text commands, fonts, anchors, wrapping, line spacing, character limit and interface scaling as `drawText`, including `ignoreInterfaceScale`. Bounds describe glyph advances and line height, not the outer visible pixels introduced by shadows or image-processing directives. Empty text has zero-sized bounds at `position`.

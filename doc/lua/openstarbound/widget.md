# Widget

[Lua API](../README.md#openstarbound-compatible-extensions) · [Base API](../widget.md) · [Modding guide](../../modding.md)

The `widget` table now contains extra bindings.

---

#### `AudioInstance` widget.playSound(`String` audio, [`int` loops = 0], [`float` volume = 1.0f])

An existing callback, now returns the played sound instance.

---

## Geometry and clipping

#### `void` widget.setDrawingOffset(`String` widgetName, `Vec2I` offset)

Sets the native drawing offset in interface pixels, additional to the widget's base position. It affects drawing and native screen geometry without changing the base position returned by `widget.getPosition`. `widget.setPosition` does not clear this offset.

#### `void` widget.setScissoringEnabled(`String` widgetName, `bool` enabled)

Enables or disables clipping to the widget's own bounds. Ancestor clipping regions still apply. This does not change visibility, size or input policy. Disable the containing pane's clipping with `pane.setScissoringEnabled(false)` when children must extend beyond its root bounds.

For pointer geometry, reuse the existing `widget.inMember(widgetName, screenPosition)` with screen coordinates in interface pixels, such as `onInput`'s `event.screenPosition`. Native hit testing includes the drawing offset and the clipping area established by rendering; an undrawn headless widget does not model that rendered area.

---

## Widget specific callbacks

These callbacks only work for some widget types.

### Live native display handles

#### `void` widget.setItemSlotHandle(`String` widgetName, [`NativeItemHandle` item])

Assigns the actual engine-produced opaque Item userdata to an ItemSlotWidget;
nil clears it. The widget shares native ownership, without descriptor
serialization/reconstruction. It does not move inventory items. Handles cannot
be stored in JSON or `storage`; count/durability continue reading the live item.

#### `void` widget.setItemSlotDisplay(`String` widgetName, `Json` display)

Updates only supplied fields: `durability`, `count`, `secondaryIcon` and
`highlight` are booleans; `countPosition` is native TextPositioning JSON
(for example `{position = {10, -6}, horizontalAnchor = "mid"}`).
Unspecified fields retain their previous settings. This changes display only.

#### `void` widget.setPortraitEntity(`String` widgetName, [`NativePortraitHandle` entity])

Assigns the actual opaque PortraitEntity userdata from
`interface.portraitEntity` to a PortraitWidget, or nil to clear. This retains
native live portrait generation/animation, not reconstructed drawable JSON.

### ProgressWidget helpers

#### `void` widget.setProgressColor(`String` widgetName, `Color` color, [`Color` mix], [`float` amount])

Sets native progress color. If `mix` is present, uses native `Color::mix` with
`amount` (default 0), retaining native color parsing and rounding.

#### `void` widget.setProgressOverlay(`String` widgetName, `String` image)

Replaces the native progress overlay image; an empty string clears it.

### LabelWidget helpers

#### `void` widget.setTextCharLimit(`String` widgetName, [`unsigned` limit])

Sets a Label's rendered Unicode-codepoint limit, excluding rich-text escape
commands; nil removes the limit. This does not truncate the stored text.


### Layout helpers

#### `void` widget.refreshLayout(`String` widgetName)

Updates a native Layout with zero elapsed time, allowing its geometry to
settle immediately after a batch of list or size changes.

---

### ImageWidget helpers

#### `void` widget.setImageDrawables(`String` widgetName, `JsonArray` drawables, [`Vec2F` scale])

Replaces an ImageWidget's native drawables. An empty array clears them.
When supplied, `scale` transforms all drawable geometry before assignment;
for example `{-1, 1}` mirrors horizontally without reversing the vertical axis.
Omitting it applies no additional scale.

An image drawable's explicit `transformation` is its complete image-local
matrix, including any centering already encoded by native portrait APIs.
Without a matrix, `centered` defaults to `true`. `position` remains a separate
drawable offset. Native drawable tables and native drawable JSON therefore
retain their image geometry when passed back through Lua.

---

### ItemGridWidget helpers

#### `void` widget.setItemGridItems(`String` widgetName, `JsonArray` items, [`Integer` capacity])

Builds a native ItemBag from item descriptors and assigns it to the grid.
Items merge and split using native stacking rules; items beyond the bag
capacity do not fit in the display bag. This does not alter player inventory.
The default capacity is the grid's visible slot count. Pass an explicit
capacity when the logical bag is larger; the bundled quest log uses **5** for
its two visible reward slots. An empty array clears the bag.
The existing `widget.itemGridItems` getter returns its native bag JSON.

Native ItemGrid/ItemSlot tooltips work automatically in script panes; custom
Lua tooltip hooks are optional.

---

### Text color

#### `void` widget.setFontColor(`String` widgetName, `Color` color, [`float` alpha])

Sets the color of a LabelWidget, ButtonWidget or TextBoxWidget. The optional
`alpha` replaces the color's alpha using native clamped float precision before
assignment, avoiding premature conversion to an eight-bit Lua color channel.
Omitting it retains the supplied color's alpha.

---

### TextBoxWidget helpers

#### `String` widget.getHint(`String` widgetName)

Gets the hint text of a TextBoxWidget.

---

#### `void` widget.setHint(`String` widgetName, `String` hint)

Sets the hint text of a TextBoxWidget.

---

#### `Int` widget.getCursorPosition(`String` widgetName)

Gets the zero-based Unicode codepoint cursor position of a TextBoxWidget (not a UTF-8 byte offset or grapheme index).

---

#### `void` widget.setCursorPosition(`String` widgetName, `int` cursorPosition)

Sets the cursor position of a TextBoxWidget, clamped to its text length, and clears its selection.

---
#### `void` widget.setTextEditorRendering(`String` widgetName, `bool` enabled)

Controls native text, selection, caret and border drawing without changing visibility, focus or input handling. Set `false` to paint the entire field using Canvas. Do not hide the widget: hidden widgets no longer request text-input capture.

#### `Json` widget.getTextEditorState(`String` widgetName)

Returns `{text, displayText, cursor, anchor, selectionStart, selectionEnd, cursorOffset}`. All indices are zero-based Unicode codepoint offsets; the ordered selection is `[selectionStart, selectionEnd)`, with `anchor` retaining its direction. `displayText` uses the engine's password masking/newline markers. `cursorOffset` is the horizontal caret offset from the text anchor in interface units, before overfill scrolling. Use `widget.setTextStyle` with the same style used by Canvas.

#### `void` widget.setTextSelection(`String` widgetName, `int` anchor, `int` cursor)

Sets the selection and caret, clamping both to the text length. Equal indices clear the selection.

#### `void` widget.setTextStyle(`String` widgetName, `Json` style)

Sets a LabelWidget or TextBoxWidget's complete text style: `fontSize`, `font`,
`color`, `shadow`, `lineSpacing`, `directives` and `backDirectives`. For a
TextBoxWidget this also controls caret measurement when native drawing is disabled.

The off-draw editor is still a real `textbox` child with a registered callback. Existing `widget.focus`, `blur`, `setText` and textbox callback/`enterKey`/`escapeKey` configuration apply. Callbacks retain their existing event semantics (including key release); scripts can also read the state while rendering. The containing pane must forward raw focused-widget events and report its keyboard capture mode/area. Set the widget's position and size to the actual input field so the platform receives the candidate-window area.

Editing includes Unicode-codepoint navigation, engine word-boundary navigation with Ctrl/Alt, Home/End, key repeat, Shift selection, Ctrl+A, clipboard copy/cut/paste and committed `TextInputEvent` insertion/replacement. Copy/cut without a selection keeps the engine's whole-field behavior. Selection replacement respects the textbox regex and width constraints. This remains a single-line field: Shift+Enter stores a newline rendered as `↵`; it is not a multiline layout editor. Combining marks/grapheme clusters are not treated as one caret step.

The SDL backend starts/stops platform text input and passes the input area/caret offset to SDL. It forwards committed text, but the engine has no composition/preedit event representation or Lua preedit API; composition display and candidate behavior remain platform/SDL-dependent. This API does not add shaping, grapheme editing, or custom preedit rendering.

---


### SliderBarWidget helpers

#### `void` widget.setSliderRange(`String` widgetName, `int` minimum, `int` maximum, [`int` step])

Sets the allowed range for a slider, optionally overriding the step used when the slider value is changed. The step defaults to
`1` when omitted.

---

### ImageStretchWidget helpers

#### `void` widget.setImageStretchSet(`String` widgetName, `Json` imageStretchSet)

Sets the full image set of a ImageStretchWidget.
```lua
{
  ["begin"] = "image.png",
  ["inner"] = "image.png",
  ["end"] = "image.png"
}
```

---

### FlowLayout helpers

#### `void` widget.addFlowImage(`String` widgetName, `String` childName, `String` imagePath)

Appends a new ImageWidget to the specified FlowLayout widget, using `childName` as the new widget's identifier within the
layout.

---

### ScrollArea helpers

#### `Maybe<Vec2I>` widget.getScrollOffset(`String` widgetName)

Gets the current scroll offset of a ScrollArea widget. Returns `nil` if the widget is not a ScrollArea.

---

#### `void` widget.setScrollOffset(`String` widgetName, `Vec2I` offset)

Sets the current scroll offset of a ScrollArea widget.

---

#### `Vec2I` widget.getMaxScrollPosition(`String` widgetName)

Gets the maximum scroll position of a ScrollArea widget. This is the maximum offset that can be scrolled to.

---

### ListWidget helpers

#### `void` widget.clearListSelected(`String` widgetName)

Clears the selected item in a ListWidget, if any.

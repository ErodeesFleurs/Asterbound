# Camera

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

The `camera` table provides functions to interact with the world camera in client-side scripts.

Camera queries and coordinate conversions describe the displayed camera,
including presentation interpolation, zoom and pixel snapping. During rendering
they match the current display sample; input between presentations uses the last
actually displayed transform. Logical entity positions remain simulation state:
do not write sampled camera or display positions back into movement controllers.

---

#### `Vec2F` camera.position()

Returns the center world position of the camera.

---

#### `Float` camera.pixelRatio()

Returns the current pixel ratio (zoom level) of the camera.

---

#### `void` camera.setPixelRatio(`Float` pixelRatio, [`Bool` smooth])

Sets the pixel ratio (zoom level) of the camera. If smooth is `true`, the camera will smoothly transition to the new zoom level.

---

#### `Vec2U` camera.screenSize()

Returns the screen size in pixels as a Vec2U.

---

#### `RectF` camera.worldScreenRect()

Returns the world rectangle visible on screen.

---

#### `RectI` camera.worldTileRect()

Returns the tile rectangle visible on screen.

---

#### `Vec2F` camera.tileMinScreen()

Returns the minimum tile size on screen.

---

#### `Vec2F` camera.screenToWorld(`Vec2F` screenPosition)

Converts a screen position to a world position.

---

#### `Vec2F` camera.worldToScreen(`Vec2F` worldPosition)

Converts a world position to a screen position.
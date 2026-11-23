# Clipboard

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

The `clipboard` table is available to client-side scripts (such as script panes and interface scripts) and exposes access to the
system clipboard while the game window is focused.

---

#### `bool` clipboard.available()

Returns `true` if clipboard access is currently allowed. Clipboard operations require the game window to be focused and, unless
overridden, for the player to have enabled clipboard integration.

---

#### `bool` clipboard.hasText()

Returns `true` if textual data is available on the clipboard and the script may read it.

---

#### `Maybe<String>` clipboard.getText()

Returns the clipboard text when access is available, or `nil` otherwise.

---

#### `Maybe<String>` clipboard.getImage()

Reads raw GIF, WebP, PNG, JPEG or BMP bytes, in that order, preferring original animation to a static
preview. Requires focus and clipboard read authorization. Returns `nil` if none is available within 8 MiB. Detect the format from bytes,
not MIME: some applications publish JPEG under `image/png`. Does not open file references.

---

#### `Maybe<String>` clipboard.imageToPng(`String` bytes)

Converts JPEG, static WebP or uncompressed BMP bytes to a PNG byte string. The format is detected
from the bytes. The input is limited to 2 MiB, 4096 pixels on either side and 4,194,304 pixels
total. Raster dimensions are preserved up to 128×128; larger images are reduced proportionally.
WebP transparency is preserved. Animated WebP is rejected rather than flattened. PNG and GIF
are not converted: use their original bytes. Returns `nil` for invalid or unsupported input.
Metadata, including EXIF orientation, is not applied or copied. This function does not access
files or the clipboard and does not require clipboard permission.

---

#### `Maybe<String>` clipboard.getFileReference()

Returns URI-list text from `text/uri-list` or `x-special/gnome-copied-files` (without the `copy`/`cut` line).
Uses the same focus and read authorization as `getImage`. Ignores payloads larger than 16 KiB and returns `nil`
if no supported list is available. References are not decoded, verified, or opened.

---

#### `bool` clipboard.setText(`String` text)

Replaces the clipboard contents with the supplied UTF-8 text. Returns `false` if the game window is unfocused or the operation is
blocked.

---

#### `bool` clipboard.setData(`Table<String, String>` data)

Replaces the clipboard contents with arbitrary data payloads. Keys are MIME type strings (for example `"text/plain"` or
`"image/png"`) and values are raw byte strings containing the payload for that type. Returns `false` if the game window is
unfocused or the operation is blocked.

---

#### `bool` clipboard.setImage(`Variant<Image, String>` imageOrPath)

Copies an image to the clipboard. When an `Image` userdata is provided it is exported as-is; when a string path is supplied the
image asset is loaded before exporting. The image is written as PNG data and `false` is returned if the clipboard cannot be
modified.

On Windows, the native clipboard also receives an RGBA32 DIB representation for applications that do not consume PNG clipboard data.
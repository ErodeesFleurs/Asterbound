# Songbook

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

The `songbook` table is accessible from player and NPC scripts, and from managed interface panes bound to the current player.

---

#### `String` songbook.band()

Returns the native playback time-source identifier. A nonempty band supplied to `play` becomes this identifier; an empty band selects a native local identifier. Defaults to `""` before playback.

---

#### `Json` songbook.song()

Returns the submitted song JSON, normally `{resource, abc}`, or null after `stop`.

---

#### `void` songbook.keepAlive(`String` instrument, `Vec2F` position)

Updates the instrument kind and position and refreshes the native playback keepalive. Instrument scripts normally maintain this while the instrument is held; opening or closing a songbook pane does not replace that responsibility.

---

#### `void` songbook.stop()

Causes the instrumentalist to stop playing.

---

#### `bool` songbook.active()

Returns whether the instrumentalist is playing a song.

---

#### `bool` songbook.instrumentPlaying()

Returns whether the instrumentalist is currently performing part of a song.

---

#### `void` songbook.play(`Json` song, `String` band)

Starts native playback. The bundled songbook submits `{resource = assetPath, abc = root.assetData(assetPath)}`, preserving the original ABC bytes. A nonempty `band` uses the native shared time source; `""` selects a local one. Song parsing, audio and playback synchronization remain native.

---

#### `String` songbook.instrument()

Returns the type of instrument last played, as determined by the instrument's `kind` value. Defaults to `""`.

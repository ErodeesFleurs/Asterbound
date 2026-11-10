# Assets

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

Accessible in onLoad scripts, postLoad scripts, and Lua asset patches. Lua asset patches receive only the read-only functions; `add`, `patch`, `patchReplace`, `loadScriptReplace`, and `erase` are exclusive to onLoad/postLoad scripts.

onLoad scripts and postLoad scripts are defined under the `scripts` key of a mod's metadata, like so:
```json
"scripts":{
  "onLoad": ["/scripts/assets/thisonloadscript.lua"],
  "postLoad": ["/scripts/assets/somepostloadscript.lua"]
}
```

onLoad and postLoad scripts also have access to `sb`. They do not have access to any other API tables, not even `root`.

onLoad and postLoad scripts work differently from other scripts. 
They do not use `init` or `update`, instead functioning like a traditional Lua script with full API access outside of functions.

---

#### `StringSet` assets.byExtension(`String` extension)

Returns a set of every asset path with this extension.

---

#### `StringList` assets.scan([`String` a], [`String` b])

If `b` is not specified, `a` is a suffix.
If both are specified, `a` is a prefix and `b` is a suffix.

Returns a list of all asset paths with with the prefix, if specified, and the suffix, if specified. This is case insensitive.
The prefix and suffix are checked against the entire path, including directories and extensions.
If neither are specified, returns ALL asset paths.

---

#### `Json` assets.json(`String` path)

Returns the JSON contents of a JSON asset file. Similar to `root.assetJson`.

---

#### Maybe<`String`> assets.origin(`String` path)

Returns the sourcePath for the specified asset or `nil` if the asset doesn't exist. Similar to `root.assetOrigin`.

---

#### `String` assets.bytes(`String` path)

Returns the raw contents of the specified asset file as a String. Similar to `root.assetData`.

---

#### `String` assets.sourceBytes(`String` sourcePath, `String` path)

Returns the exact raw bytes of `path` from the indexed asset source identified by `sourcePath` (as returned by `assets.sourcePaths` or `assets.origin`). Unlike `assets.bytes`, this reads that provider even when another source shadows the same virtual path. It does not apply patches or use the asset cache.

Only paths enumerated by that source are accessible; this is not an arbitrary filesystem reader. An unknown source, an unindexed path, or a read failure raises an error identifying the requested path and source. This function is also available in read-only Lua asset patch contexts.

---

#### `Image` assets.image(`String` path)

Returns the specified image asset as an `Image`. Similar to `root.assetImage`.

---

#### `Json` assets.frames(`String` path)

Returns the frame data of the specified image asset. Similar to `root.assetFrames`.

---

#### `bool` assets.exists(`String` path)

Returns true if the asset under the specified path exists.

---

#### `void` assets.add(`String` path, `LuaValue` data)

Adds a new asset at the specified path with the specified data. If an asset already exists here, it is overwritten.

---

#### `bool` assets.patch(`String` path, `String` patchPath)

Patches the specified asset with the specified patch file. Returns true if successful.

---

#### `bool` assets.patchReplace(`String` target, `String` originalPatchPath, `String` sourcePath, `String` replacementPatchPath, [`String` replacementTarget])

Replaces every registered patch slot on `target` whose original patch path and held asset source both match `originalPatchPath` and `sourcePath`. The original patch path matches exactly, including any subpath. Without a different `replacementTarget`, slots stay in their original order. With a different existing target, the matched slots are removed from the original target and the same number of replacement registrations are appended after all existing destination patches. All unrelated source and destination registrations retain their order, including patches from other providers at the same virtual path.

The replacement comes from assets added in the current script group if present, otherwise from the currently indexed provider of `replacementPatchPath`. It replaces the registered patch itself, not merely the globally visible patch file. The next read in the same script context observes the replacement, including after an earlier failed read.

Returns `false` without changing registrations or caches if either target, the matching original registration/source, or the replacement asset is unknown. Returns `true` after replacing or moving matching slots; patch parsing and execution still happen on asset read and retain normal failure behavior and actual replacement-provider diagnostics. Available only in onLoad/postLoad scripts.

---

#### `bool` assets.loadScriptReplace(`String` sourcePath, `String` originalScriptPath, `String` replacementScriptPath)

Replaces future executions of the exact `originalScriptPath` declared in the indexed source's metadata `scripts.onLoad` or `scripts.postLoad`. The original must be one of that source's enumerated assets. This changes only the held source/path used by the asset-loading script runner; it does not override a virtual asset or redirect ordinary game scripts, `require`, or another source's same-named script. Original declaration order and shared script-group context are preserved.

The replacement comes from assets added in the current script group if present, otherwise from the currently indexed replacement asset provider. Failures identify the actual replacement script path and provider and retain the normal load-script failure policy.

Returns `false` without mutation for unknown sources, undeclared/unindexed originals, unavailable replacements, or an original source/path whose execution has already started (including the currently executing script). Declarations skipped after an earlier failure in their group are also unavailable. Returns `true` after registering a replacement for future declarations; calling again before execution updates that replacement. Available only in onLoad/postLoad scripts.

---

#### `bool` assets.erase(`String` path)

Removes the specified asset. Returns true if successful.

---

#### `LuaTable` assets.sourcePaths([`bool` withMetadata])

If withMetadata is false or unspecified, returns a list of the paths of all asset sources relative to the game executables.
If withMetadata is true, returns a map of all asset source paths to the contents of their metadatas or an empty table if the source has no metadata.

---

#### Maybe<`JsonObject`> assets.sourceMetadata(`String` sourcePath)

Returns the metadata of the specified sourcePath, an empty table if the sourcePath lacks a metadata, or `nil` if it doesn't exist.

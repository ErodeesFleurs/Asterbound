---@meta

--- assets API
---@class assets
assets = {}

--- Returns a set of every asset path with this extension. ---
---@param extension string
---@return StringSet
function assets.byExtension(extension) end

--- If `b` is not specified, `a` is a suffix. If both are specified, `a` is a prefix and `b` is a suffix. Returns a list of all asset paths with with the prefix, if specified, and the suffix, if specified. This is case insensitive. The prefix and suffix are checked against the entire path, including directories and extensions. If neither are specified, returns ALL asset paths. ---
---@param a string
---@param b string
---@return StringList
function assets.scan(a, b) end

--- Returns the JSON contents of a JSON asset file. Similar to `root.assetJson`. --- #### Maybe<`String`> assets.origin(`String` path) Returns the sourcePath for the specified asset or `nil` if the asset doesn't exist. Similar to `root.assetOrigin`. ---
---@param path string
---@return Json
function assets.json(path) end

--- Returns the raw contents of the specified asset file as a String. Similar to `root.assetData`. ---
---@param path string
---@return string
function assets.bytes(path) end

--- Reads exact raw bytes from an indexed source, including shadowed assets. Only that source's enumerated paths are accessible. Unknown sources, unindexed paths, and read failures raise source/path-specific errors. Also available in read-only Lua asset patches.
---@param sourcePath string
---@param path string
---@return string
function assets.sourceBytes(sourcePath, path) end

--- Returns the specified image asset as an `Image`. Similar to `root.assetImage`. ---
---@param path string
---@return Image
function assets.image(path) end

--- Returns the frame data of the specified image asset. Similar to `root.assetFrames`. ---
---@param path string
---@return Json
function assets.frames(path) end

--- Returns true if the asset under the specified path exists. ---
---@param path string
---@return boolean
function assets.exists(path) end

--- Adds a new asset at the specified path with the specified data. If an asset already exists here, it is overwritten. ---
---@param path string
---@param data LuaValue
function assets.add(path, data) end

--- Patches the specified asset with the specified patch file. Returns true if successful. ---
---@param path string
---@param patchPath string
---@return boolean
function assets.patch(path, patchPath) end

--- Load scripts only. Replaces every held patch registration matching the exact original path and source, in place by default. A different replacementTarget removes matching source slots and appends replacement copies after existing destination patches, preserving all unrelated order. Uses current-group added assets before the indexed replacement provider. Missing either target/original/source/replacement returns false without mutation. Subsequent reads see the replacement; normal patch errors remain errors.
---@param target string
---@param originalPatchPath string
---@param sourcePath string
---@param replacementPatchPath string
---@param replacementTarget? string
---@return boolean
function assets.patchReplace(target, originalPatchPath, sourcePath, replacementPatchPath, replacementTarget) end

--- Load scripts only. Replaces future metadata onLoad/postLoad executions for one exact indexed source/path, preserving declaration order and group context. Uses current-group assets before indexed replacements; does not redirect game scripts or other providers. Unknown, undeclared, unindexed, started, or skipped declarations and missing replacements return false without mutation. Failures retain actual replacement provenance.
---@param sourcePath string
---@param originalScriptPath string
---@param replacementScriptPath string
---@return boolean
function assets.loadScriptReplace(sourcePath, originalScriptPath, replacementScriptPath) end

--- Removes the specified asset. Returns true if successful. ---
---@param path string
---@return boolean
function assets.erase(path) end

--- If withMetadata is false or unspecified, returns a list of the paths of all asset sources relative to the game executables. If withMetadata is true, returns a map of all asset source paths to the contents of their metadatas or an empty table if the source has no metadata. --- #### Maybe<`JsonObject`> assets.sourceMetadata(`String` sourcePath) Returns the metadata of the specified sourcePath, an empty table if the sourcePath lacks a metadata, or `nil` if it doesn't exist.
---@param withMetadata boolean
---@return LuaTable
function assets.sourcePaths(withMetadata) end

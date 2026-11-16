---@meta

--- Generates an inline binary asset path, preserving every byte including NUL.
--- Generation is independent of safe.allowRemoteAssets; loading requires that setting (default true).
--- Raw script reads require root.assetData(path, true). No downloads or disk storage.
--- Compatible engines only, not vanilla/older clients. Decoded payload and PNG expanded RGBA limits: 64 MiB each; JSON depth: 256.
--- Compression is ASCII case-insensitive none/zlib/zstd; gzip is decode-only.
---@param data string
---@param compression string
---@return string
function sb.embedData(data, compression) end

--- Generates an inline asset using the engine's binary JSON stream format.
--- Compression is ASCII case-insensitive none/zlib/zstd; gzip is decode-only.
---@param data Json
---@param compression string
---@return string
function sb.embedJson(data, compression) end

--- Generates an escaped plaintext inline asset path without compression.
---@param data string
---@return string
function sb.embedText(data) end

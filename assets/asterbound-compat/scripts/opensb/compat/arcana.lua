-- Arcana release v1.4.2.10 (its GitHub metadata still says 1.4.2.9):
-- https://github.com/sleepySva/sva_arcana/commit/2b7bc3a0b1e32bd0dcb73e2e4042ff8f04f9c409
-- Official Workshop 2359135864 metadata is 1.4.2.10; its target script is
-- identical to that commit after CRLF-to-LF normalization.
-- dump quotes nonnumeric keys by assigning to the first generic-for variable,
-- which is read-only in Lua 5.5. Keep iteration keys separate from display keys.
local path = "/scripts/arcana_playereffects.lua"
local source = assets.origin(path)
if not source then return end

local metadata = assets.sourceMetadata(source)
if not metadata or metadata.name ~= "Arcana" then return end
if metadata.version ~= "1.4.2.9" and metadata.version ~= "1.4.2.10" then
  sb.logWarn("[Asterbound Compatibility] Arcana playereffects has an unsupported version; left unchanged")
  return
end

local script = assets.bytes(path)
local normalized = script:gsub("\r\n", "\n")
-- FNV-1a/64 plus length guards the entire known source, not just the offending
-- line. Only line-ending differences are accepted; preserve original bytes.
-- Original LF SHA-256: cfb4bee2fa92ccbf31988afb2c5c31a36ad3067180b164be2c599bce7606323e
local fingerprint = 0xcbf29ce484222325
for i = 1, #normalized do
  fingerprint = (fingerprint ~ normalized:byte(i)) * 0x100000001b3
end
if #normalized ~= 3112 or fingerprint ~= 0x572143e963e8c4ad then
  sb.logWarn("[Asterbound Compatibility] Arcana playereffects has unrecognized source bytes; left unchanged")
  return
end

local newline = script:find("\r\n", 1, true) and "\r\n" or "\n"
local before = "      for k,v in pairs(o) do" .. newline
local first, last = script:find(before, 1, true)
if not first or script:find(before, last + 1, true) then
  sb.logWarn("[Asterbound Compatibility] Arcana playereffects has an unrecognized dump loop; left unchanged")
  return
end
local after = "      for originalKey,v in pairs(o) do" .. newline
    .. "         local k = originalKey" .. newline
assets.add(path, script:sub(1, first - 1) .. after .. script:sub(last + 1))
sb.logInfo("[Asterbound Compatibility] Migrated Arcana dump display keys to a local working variable for Lua 5.5")

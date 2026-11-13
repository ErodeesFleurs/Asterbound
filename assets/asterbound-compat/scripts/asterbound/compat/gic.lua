-- Official Galaxy in Conflict Workshop 2754886445, metadata v4.3.2a.
-- Fingerprints cover complete original provider bytes after CRLF-to-LF
-- normalization only. Preserve every unrelated byte and original line ending.
local function repair(path, size, expected, before, after, requiredAsset)
  local source = assets.origin(path)
  if not source then return end
  local metadata = assets.sourceMetadata(source)
  if not metadata or metadata.name ~= "Galaxy_in_Conflict" then return end
  if metadata.version ~= "v4.3.2a" then
    sb.logWarn("[Asterbound Compatibility] GIC %s has an unsupported version; left unchanged", path)
    return
  end
  if requiredAsset and assets.origin(requiredAsset) ~= source then return end

  local bytes = assets.sourceBytes(source, path)
  local normalized = bytes:gsub("\r\n", "\n")
  local fingerprint = 0xcbf29ce484222325
  for i = 1, #normalized do
    fingerprint = (fingerprint ~ normalized:byte(i)) * 0x100000001b3
  end
  if #normalized ~= size or fingerprint ~= expected then
    sb.logWarn("[Asterbound Compatibility] GIC %s has unrecognized source bytes; left unchanged", path)
    return
  end

  local newline = bytes:find("\r\n", 1, true) and "\r\n" or "\n"
  before = before:gsub("\n", newline)
  after = after:gsub("\n", newline)
  local first, last = bytes:find(before, 1, true)
  -- Exactly one original fragment and no already-repaired fragment are allowed.
  if not first or bytes:find(before, last + 1, true) or bytes:find(after, 1, true) then
    sb.logWarn("[Asterbound Compatibility] GIC %s has an unrecognized repair site; left unchanged", path)
    return
  end
  assets.add(path, bytes:sub(1, first - 1) .. after .. bytes:sub(last + 1))
  sb.logInfo("[Asterbound Compatibility] Repaired GIC asset %s", path)
end

-- Normalized SHA-256: ca2012f06136a5e90ae22423102adce6532f412f7196f98ad09ba5337e1ad98c.
-- Keep the integral iteration range and clamp its local working value, including
-- fractional min/max endpoints, exactly as the original Lua 5.3 code did.
repair("/stagehands/coordinator/gic_npccombat.lua", 9221, 0xe6a08c3faaf2230d,
  "  for range = math.floor(minRange), math.ceil(maxRange) do\n"
    .. "    range = math.max(minRange, math.min(maxRange, range))",
  "  for originalRange = math.floor(minRange), math.ceil(maxRange) do\n"
    .. "    local range = math.max(minRange, math.min(maxRange, originalRange))")

-- Normalized SHA-256: 30b046a95a03cf34e29665b36574c413a489b768ccf0537f12d255bc4f63da9a.
-- The iterator remains one-based; drawing and damage indexing use its local
-- zero-based working index without assigning to Lua 5.5's read-only variable.
repair("/interface/games/gic_bombdefusalgame/tileset.lua", 7167, 0x843c2874f21549c2,
  "  for i,value in ipairs(self.tiles) do\n"
    .. "    i = i - 1 -- Lua is 1-indexed but we want to start at 0",
  "  for originalIndex,value in ipairs(self.tiles) do\n"
    .. "    local i = originalIndex - 1 -- Lua is 1-indexed but we want to start at 0")

-- Normalized SHA-256: 72b637011dde95090c6a2742426a9f000bf9b2563f6bb8516697cfb25a6e17b9.
-- Separate the malformed numeric token from the keyword; no missile logic changes.
repair("/vehicles/gic_sika_helicopter/gic_sika_helicopter_updated.lua", 55521, 0xbfd3e3dbf00e64c4,
  '\telseif (vehicle.controlHeld("copilotSeat","AltFire")) and storage.missileLoaded > 0 and self.missileFireTimer == 0then',
  '\telseif (vehicle.controlHeld("copilotSeat","AltFire")) and storage.missileLoaded > 0 and self.missileFireTimer == 0 then')

-- Normalized SHA-256: 2827a5e86d5f4790d25a7cec943647706b00e45378d37b8bf7fbd409246c5f2d.
-- Retain the skeleton's silent hurt response using GIC's own existing silent
-- sound, while satisfying the species database's two gender sound references.
repair("/species/gic_undeadskeleton.species", 8577, 0xd2b9937236471460,
  '  "ouchNoises" : [ ],',
  '  "ouchNoises" : [ "/sfx/gic_silence.ogg", "/sfx/gic_silence.ogg" ],',
  "/sfx/gic_silence.ogg")

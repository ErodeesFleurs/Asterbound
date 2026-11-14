-- Official Workshop 729480149, FrackinUniverse 6.5.8. Repair only the five
-- mounted scripts below; do not migrate backups or standalone shell tools.
-- Full-source fingerprints normalize CRLF to LF only. Every replacement keeps
-- the provider's line endings and all unrelated bytes, including pcall errors.
local function repair(path, size, expected, replacements)
  local source = assets.origin(path)
  if not source then return end
  local metadata = assets.sourceMetadata(source)
  if not metadata or metadata.name ~= "FrackinUniverse" then return end
  if metadata.version ~= "6.5.8" then
    sb.logWarn("[Asterbound Compatibility] FrackinUniverse %s has an unsupported version; left unchanged", path)
    return
  end

  local bytes = assets.sourceBytes(source, path)
  local normalized = bytes:gsub("\r\n", "\n")
  local fingerprint = 0xcbf29ce484222325
  for i = 1, #normalized do
    fingerprint = (fingerprint ~ normalized:byte(i)) * 0x100000001b3
  end
  if #normalized ~= size or fingerprint ~= expected then
    sb.logWarn("[Asterbound Compatibility] FrackinUniverse %s has unrecognized source bytes; left unchanged", path)
    return
  end

  local newline = bytes:find("\r\n", 1, true) and "\r\n" or "\n"
  for _, replacement in ipairs(replacements) do
    local before = replacement[1]
    local after = replacement[2]:gsub("\n", newline)
    local escaped = before:gsub("(%W)", "%%%1")
    local repaired, count = bytes:gsub(escaped, function() return after end)
    if count ~= 1 then
      sb.logWarn("[Asterbound Compatibility] FrackinUniverse %s has an unrecognized repair site; left unchanged", path)
      return
    end
    bytes = repaired
  end
  -- Combine both router repairs before publishing: same-group adds are not yet
  -- visible, and another sourceBytes read would discard the first repair.
  assets.add(path, bytes)
  sb.logInfo("[Asterbound Compatibility] Applied guarded FrackinUniverse source repair to %s", path)
end

-- LF SHA-256: dbe1fba72223ad0edc6af8d18e25038fe4fd20cd26423d09e4b9bc1d358d3772
-- Discard the pcall success flag in an if-local, not the read-only loop index.
-- Keep its second result (including the error value on failure) in self.racial.
repair("/scripts/fu_player_init.lua", 13483, 0xd009238ea0dc8ed4, {
  {
    "\t\t\t\t\t\t_,self.racial = pcall(function (f) return root.assetJson(\"/frackinraces.config\").manipulators end)",
    "\t\t\t\t\t\tlocal _\n\t\t\t\t\t\t_,self.racial = pcall(function (f) return root.assetJson(\"/frackinraces.config\").manipulators end)"
  }
})

-- LF SHA-256: 049970c84bd403d31b9fd555dfcfe3729e33f29a59e36f114640bb9f3dfb7c8d
-- Preserve boolean/quoted-string argument conversion with a per-iteration local.
repair("/zb/newSail/newSail.lua", 39041, 0x32195e20ef3f7544, {
  {
    "\t\t\t\t\t\tfor k in (argStr):gmatch(\"[^,]+\") do",
    "\t\t\t\t\t\tfor argument in (argStr):gmatch(\"[^,]+\") do\n\t\t\t\t\t\t\tlocal k = argument"
  }
})

-- LF SHA-256: 2dbf11ea01f6bb26b4505d7556376c5fa607ff6c04d2d5ff0dfd976fdd2aa0a0
-- The awakened/replaced container ID is needed only within this awake branch.
repair("/objects/kheAA/kheAA_terminal/kheAA_terminal.lua", 1549, 0x9a740cd7bba29445, {
  {
    "            id = newId or id",
    "            local id = newId or id"
  }
})

-- LF SHA-256: f3488c62b8c0912e40715c1d64779537d795c7818b76c42a753cf5f600057f7f
-- Both the source and nested target may acquire new entity IDs after waking.
-- Their writable copies retain exactly the original iteration-body scopes.
repair("/objects/kheAA/kheAA_router/kheAA_router.lua", 11189, 0x92c244eef513e2b0, {
  {
    "\tfor sourceContainer,sourcePos in pairs(transferUtil.vars.inContainers or {}) do",
    "\tfor sourceId,sourcePos in pairs(transferUtil.vars.inContainers or {}) do\n\t\tlocal sourceContainer = sourceId"
  },
  {
    "\t\t\t\t\t\tfor targetContainer,targetPos in pairs(transferUtil.vars.outContainers or {}) do",
    "\t\t\t\t\t\tfor targetId,targetPos in pairs(transferUtil.vars.outContainers or {}) do\n\t\t\t\t\t\t\tlocal targetContainer = targetId"
  }
})

-- LF SHA-256: 5ce66b798bb83d9a1c3de63c5b1dc6641f4fa8aeb0c9b5f7de9ac3808655bd72
-- FR scripts register FRHelper:call; loadScript caches that callable separately
-- from successful loading. Track successes privately rather than consulting the
-- old engine-internal _SBLOADED global. A successful script without a callback
-- is still loaded; a throwing require is not marked and remains retryable.
repair("/scripts/FRHelper.lua", 10038, 0x394b7e1cb6307466, {
  {
    "DynamicScripts = {}",
    "DynamicScripts = {}\nlocal loadedScripts = {}"
  },
  {
    "\t\t\tDynamicScripts[path] = self.call\t -- Add to loaded script list",
    "\t\t\tDynamicScripts[path] = self.call\t -- Add to loaded script list\n\t\t\tloadedScripts[path] = true"
  },
  {
    "\treturn _SBLOADED[script] or false",
    "\treturn loadedScripts[script] or false"
  }
})

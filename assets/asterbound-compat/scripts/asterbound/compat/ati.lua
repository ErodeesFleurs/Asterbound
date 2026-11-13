-- Advanced Teleporter Interface 1.1.1 reads optional object scripts while
-- inspecting teleporters. Missing scripts must remain visible as warnings,
-- without embedding complete native exception stacks in each warning.
for _, source in ipairs(assets.sourcePaths()) do
  local metadata = assets.sourceMetadata(source)
  if metadata and metadata.name == "advanced_teleporter_interface" then
    if metadata.version ~= "1.1.1" then
      sb.logWarn("[Asterbound Compatibility] ATI bridge has an unsupported version; left unchanged")
      return
    end

    local path = "/scripts/ati/bridge/postload.lua"
    local script = assets.sourceBytes(source, path)
    local normalized = script:gsub("\r\n", "\n")
    -- Original Workshop 3788629855 SHA-256:
    -- ea138f273b11154d1556e01f7fc08c81baafd96da9202bb799d738a935eb8b8c
    local fingerprint = 0xcbf29ce484222325
    for i = 1, #normalized do
      fingerprint = (fingerprint ~ normalized:byte(i)) * 0x100000001b3
    end
    if #normalized ~= 8808 or fingerprint ~= 0xa397e77b921242e3 then
      sb.logWarn("[Asterbound Compatibility] ATI bridge has unrecognized source bytes; left unchanged")
      return
    end

    local repaired, prechecks = normalized:gsub("local function readScript%(path%)\n", function()
      return 'local function readScript(path)\n'
        .. '  if not assets.exists(path) then return nil, "No such script asset" end\n'
    end, 1)
    local diagnostics
    repaired, diagnostics = repaired:gsub("return nil, tostring%(value%)", function()
      return 'local message = tostring(value)\n'
        .. '  sb.logDebug("[ATI bridge] Could not inspect %s:\\n%s", path, message)\n'
        .. '  return nil, message:match("[^\\r\\n]*")'
    end)
    assert(prechecks == 1 and diagnostics == 2, "ATI bridge repair boundaries changed")
    if script:find("\r\n", 1, true) then
      repaired = repaired:gsub("\n", "\r\n")
    end

    local replacement = "/scripts/asterbound/compat/generated/ati-postload.lua"
    assets.add(replacement, repaired)
    assert(assets.loadScriptReplace(source, path, replacement), "ATI bridge load script could not be replaced")
    sb.logInfo("[Asterbound Compatibility] ATI bridge keeps concise inspection warnings and full Debug diagnostics")
    return
  end
end

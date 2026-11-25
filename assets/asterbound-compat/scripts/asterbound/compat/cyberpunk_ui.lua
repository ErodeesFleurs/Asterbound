-- Workshop 3536001327 (1.1). Retain themed art while restoring presentation
-- that the old native chat and the shorter stock Options layout used to own.
local name, version = "Cyberpunk 77 UI and HUD", "1.1"
local function known(path, owner, release, size, expected, binary)
  local source = assets.origin(path)
  local meta = source and assets.sourceMetadata(source)
  if not meta or meta.name ~= owner or meta.version ~= release then return end
  local data = assets.sourceBytes(source, path)
  if not binary then data = data:gsub("\r\n", "\n") end
  if #data ~= size then return end
  local hash = 0xcbf29ce484222325
  for i = 1, #data do hash = (hash ~ data:byte(i)) * 0x100000001b3 end
  if hash == expected then return data end
end

local function adaptChat()
  local legacyPatch = "/interface/chat/chat.config.patch"
  if not known(legacyPatch, name, version, 288, 0x3441becdbdcdc669) then return end
  local config = "/interface/asterbound/chat/chat.config"
  local script = "/interface/asterbound/chat/chat.lua"
  if not known(config, "asterbound_base", "0.1.4.0", 675, 0x451718c42cf4a8ad) then return end
  local data = known(script, "asterbound_base", "0.1.4.0", 17041, 0xe8215b507ffea3ac)
  if not data then return end

  local function replace(before, after)
    local first, last = data:find(before, 1, true)
    if not first or data:find(before, last + 1, true) then return false end
    data = data:sub(1, first - 1) .. after .. data:sub(last + 1)
    return true
  end
  if not replace('inputStyle.color = "green"', 'inputStyle.color = "#fbe12a"')
      or not replace('local green = {0, 255, 0, alpha}', 'local editorColor = {251, 225, 42, alpha}')
      or not replace('}, 8, green)', '}, 8, {210, 67, 63, alpha})')
      or not replace('textStyle.color = green', 'textStyle.color = editorColor')
      or not replace(', 9}, green)', ', 9}, editorColor)') then return end

  local patch = "/scripts/asterbound/compat/cyberpunk_chat.config.patch"
  assets.add(patch, [=[[
    {"op":"test","path":"/gui/panefeature/offset","value":[0,5]},
    {"op":"replace","path":"/gui/panefeature/offset","value":[20,5]}
  ]]=])
  if not assets.patch(config, patch) then return end
  assets.add(script, data)
  sb.logInfo("[Asterbound Compatibility] Adapted Cyberpunk chat appearance to the managed pane")
end

local function adaptOptions()
  local prefix = "/interface/optionsmenu/"
  local body = prefix .. "body_blank.png"
  if not known(prefix .. "optionsmenu.config", "base", nil, 4854, 0xca72b750ee033b9c)
      or not known(prefix .. "optionsmenu.config.patch", "asterbound_base", "0.1.4.0", 2447, 0x9ccd60e0a68514a0)
      or not known(prefix .. "header.png", name, version, 4434, 0xe5ec59df2b9b38c8, true)
      or not known(prefix .. "footer.png", name, version, 3889, 0x4ac4eed662faa447, true)
      or not known(body, name, version, 17140, 0x82d57a96d81067f6, true) then return end
  local original = assets.image(body)
  local size = original:size()
  if size[1] ~= 236 or size[2] ~= 142 then return end
  -- The original 142-pixel body ends below the volume heading. Extend up to
  -- the Voice section without moving native controls or replacing the header.
  local taller = original:process("?crop=0;140;236;141?scalenearest=1;188")
  taller:copyInto({0, 0}, original)
  assets.add(body, taller)
  sb.logInfo("[Asterbound Compatibility] Extended Cyberpunk Options artwork for the Voice controls")
end

adaptChat()
adaptOptions()

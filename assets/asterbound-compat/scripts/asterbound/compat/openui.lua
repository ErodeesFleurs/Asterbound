-- OpenUI 1.1.2 predates the owned-path migration and the unified control editor.
-- Its old patches have no registered slots on these panes. Attach to the actual
-- controls, using the installed mod's art; never publish an old engine namespace.
local old, live = "/interface/opensb/", "/interface/asterbound/"
local private = "/scripts/asterbound/compat/openui/"
local options = "/interface/optionsmenu/"
local source = assets.origin(old .. "bindings/bindings.config.patch")
local metadata = source and assets.sourceMetadata(source)
if not metadata or metadata.name ~= "open_ui" or metadata.version ~= "1.1.2" then return end
local base = assets.origin(live .. "bindings/bindings.config")
local baseMeta = base and assets.sourceMetadata(base)
if not baseMeta or baseMeta.name ~= "asterbound_base" or baseMeta.version ~= "0.1.4.0" then return end

local function bytes(owner, path, size, expected, held)
  if not held and assets.origin(path) ~= owner then return end
  local ok, raw = pcall(assets.sourceBytes, owner, path)
  if not ok then return end
  local text = path:sub(-4) == ".png" and raw or raw:gsub("\r\n", "\n")
  if #text ~= size then return end
  local hash = 0xcbf29ce484222325
  for i = 1, #text do hash = (hash ~ text:byte(i)) * 0x100000001b3 end
  if string.format("%016x", hash) == expected then return raw end
end

local function images(dir, manifest)
  for _, entry in ipairs(manifest) do
    if not bytes(source, dir .. entry[1] .. ".png", entry[2], entry[3]) then return false end
  end
  return true
end
if not images(options, {{"bigbuttonlong",349,"816bb3710f9e7d45"},{"bigbuttonlonghover",352,"ec104fb349154499"},
    {"bigbuttonshort",313,"519a101f344e3673"},{"bigbuttonshorthover",315,"fd4308e8cd134c3e"}}) then return end

local function add(ops, path, value)
  ops[#ops + 1] = {op = "add", path = path, value = value}
end

-- Test only the paths we style. An earlier overlay on an unrelated widget is
-- kept; a conflicting layout is left entirely intact (one patch transaction).
local function attach(name, path, baseline, ops)
  local checks, seen = jarray(), {}
  for _, op in ipairs(ops) do
    if not seen[op.path] then
      seen[op.path] = true
      local v = baseline
      for key in op.path:gmatch("[^/]+") do
        if type(v) ~= "table" then v = nil; break end
        local index = tonumber(key)
        v = v[index and index + 1 or key]
      end
      checks[#checks + 1] = {op = "test", path = op.path, value = v, inverse = v == nil}
    end
  end
  for _, op in ipairs(ops) do checks[#checks + 1] = op end
  local patch = private .. name .. ".patch"
  assets.add(patch, checks)
  assert(assets.patch(path, patch))
end

local function chrome(ops, dir)
  for field, file in pairs({fileHeader="header",fileBody="body",fileFooter="footer"}) do
    add(ops, "/gui/background/" .. field, dir .. file .. ".png")
  end
end

local function bindings()
  local dir, target = old .. "bindings/", live .. "bindings/bindings.config"
  local layout = bytes(base, target, 8773, "88071559c9eb4efa")
  local original = bytes(source, dir .. "bindings.config.patch", 1941, "09cafc31ae86ef51")
  if not layout or not original or not bytes(source, dir .. "bindings.lua", 10281, "5b0a8704b5a8af5d")
      or not bytes(base, live .. "bindings/bindings.lua", 10104, "bc8151d79a396dce")
      or not images(dir, {{"header",475,"d05973f856d6e33e"},{"body",2861,"12f3c942479d0adb"},{"footer",525,"b612e8c398263d0f"},
          {"bindname",201,"d973dab9eca4a030"},{"categoryback",201,"6f64a5872c0b9a40"},{"groupname",209,"60b40a1c0988f23e"},
          {"reset",157,"f8942e192b8bb2c4"}}) then return end
  local ops = jarray()
  for _, op in ipairs(sb.parseJson(original)) do
    if op.path == "/gui/title" then op.value.value = "CONTROL SETTINGS" end
    -- Asterbound's banner is a label, not a canvas. Its live source label
    -- distinguishes game/mod categories instead of the old static 'Mods' label.
    if op.path ~= "/gui/banner/rect" and op.path ~= "/gui/modsLabel" then ops[#ops + 1] = op end
  end
  chrome(ops, dir)
  add(ops, "/gui/banner/position", {286,228})
  add(ops, "/gui/banner/fontSize", 8)
  add(ops, "/gui/source/position", {88,228})
  add(ops, "/gui/close/position", {225,27})
  add(ops, "/gui/close/pressed", options .. "bigbuttonlonghover.png")
  for i, button in ipairs({"cancelGame", "resetGame"}) do
    local key = "/gui/" .. button .. "/"
    add(ops, key .. "position", {24 + (i - 1) * 94,27})
    for _, state in ipairs({"base","hover","pressed"}) do
      add(ops, key .. state, options .. (state == "base" and "bigbuttonshort.png" or "bigbuttonshorthover.png"))
    end
  end
  add(ops, "/gui/gameActionsLabel/position", {24,17})
  add(ops, "/gui/savePolicy/position", {217,17})
  local schema = "/gui/categories/children/list/schema/"
  for state, directive in pairs({selectedBG="?multiply=0f0",unselectedBG="?multiply=888",hoverBG="?brightness=35"}) do
    add(ops, schema .. state, dir .. "categoryback.png" .. directive)
  end
  add(ops, schema .. "memberSize", {130,16})
  add(ops, schema .. "listTemplate/label/position", {65,8})
  add(ops, "/groupRow/file", dir .. "groupname.png")
  add(ops, "/actionRow/children/background/file", dir .. "bindname.png")
  for _, state in ipairs({"base","hover","pressed"}) do
    local shade = state == "base" and "" or (state == "hover" and "?multiply=faa" or "?multiply=f00")
    add(ops, "/actionRow/children/reset/" .. state, dir .. "reset.png" .. shade)
    add(ops, "/actionRow/children/erase/" .. state, live .. "bindings/garbage.png" .. shade)
    add(ops, "/actionRow/children/apply/" .. state, live .. "bindings/bind.png" .. (state == "base" and "" or "?brightness=35"))
  end
  for name, x in pairs({apply=109,erase=210,reset=226}) do add(ops, "/actionRow/children/" .. name .. "/position", {x,3}) end
  attach("bindings", target, sb.parseJson(layout), ops)
end

local function shaders()
  local dir, target = old .. "shaders/", live .. "shaders/shaders.config"
  local layout = bytes(base, target, 5945, "e5c2f8bcd945919b")
  local script = bytes(base, live .. "shaders/shaders.lua", 10227, "90f4b514df5a43d4")
  local original = bytes(source, dir .. "shaders.config.patch", 1915, "1e139e999fb1bc6c")
  if not layout or not script or not original or not bytes(source, dir .. "shaders.lua", 10389, "7f63d39437ba6328")
      or not images(dir, {{"header",475,"d05973f856d6e33e"},{"body",2861,"12f3c942479d0adb"},{"footer",525,"b612e8c398263d0f"},
        {"groupback",201,"6f64a5872c0b9a40"},{"optionname",201,"d973dab9eca4a030"},{"select",156,"0a584134133ea219"}}) then return end
  -- Only port OpenUI's presentation. Keep signed slider ranges, group toggles
  -- and the no-shaders state from the current editor.
  local n, hover, banner
  script, n = script:gsub('local PATH = "/interface/asterbound/shaders/"', 'local PATH = "' .. dir .. '"')
  script, hover = script:gsub("%?brightness=%-25", "?brightness=50")
  script, banner = script:gsub('(bannerOptions:drawText%([^\n]-}, )16%)', '%18)')
  if n ~= 1 or hover ~= 2 or banner ~= 1 then return end
  local ops = sb.parseJson(original)
  chrome(ops, dir)
  add(ops, "/gui/noShadersMessage/position", {38,96})
  local schema = "/gui/groups/children/list/schema/"
  add(ops, schema .. "selectedBG", dir .. "groupback.png?multiply=0f0")
  add(ops, schema .. "unselectedBG", dir .. "groupback.png?multiply=222")
  add(ops, schema .. "listTemplate/background/file", dir .. "groupback.png?multiply=222")
  ops[#ops + 1] = {op="replace",path="/scripts/0",value=private .. "shaders.lua"}
  assets.add(private .. "shaders.lua", script)
  attach("shaders", target, sb.parseJson(layout), ops)
end

local function voice()
  local dir, dest = old .. "voicechat/", live .. "voicechat/"
  local layout = bytes(base, dest .. "voicechat.config", 5523, "6a49a854d51533c8")
  local locale = bytes(base, dest .. "locale.json", 341, "ed47bbbd5ecf94f1")
  local original = bytes(source, dir .. "voicechat.config.patch", 5411, "e59ba790f3c974ee")
  local words = bytes(source, dir .. "locale.json.patch", 790, "f5675bb5dfa7015b")
  local script = bytes(source, dir .. "voicechat.lua", 8175, "1983e0c2c05fdce7")
  if not layout or not locale or not original or not words or not script or not bytes(base, dest .. "voicechat.lua", 8122, "171c7ada1d0d3cc8")
      or not images(dir, {{"header",366,"4b63adb4676dde85"},{"body",886,"947ef2197ac086d4"},{"footer",333,"92abb2e48d029919"},
        {"device",153,"6b4ebeb373f8c8f7"},{"deviceback",160,"1c79ec33ba7b71f1"}}) then return end
  -- The authored script handles the 125px-wide meters. Point it at the real
  -- locale/layout and remove the three image updates for widgets the theme removes.
  local n
  script, n = script:gsub('local PATH = "/interface/opensb/voicechat/"', 'local PATH = "' .. dest .. '"')
  if n ~= 1 then return end
  -- Format only the two meter captions; stored volume and dB stay untouched.
  script, n = script:gsub('fmt%("%^shadow;%%s%%%%",', function() return 'fmt("^shadow;%.1f%%",' end)
  if n ~= 1 then return end
  script, n = script:gsub('fmt%("%^shadow;%%s dB",', function() return 'fmt("^shadow;%.1f dB",' end)
  if n ~= 1 then return end
  for _, widget in ipairs({"enableVoiceToggleBack","pushToTalkBack","voiceActivityBack"}) do
    script, n = script:gsub('\twidget%.setImage%("' .. widget .. '",[^\n]*\n', '')
    if n ~= 1 then return end
  end
  local ops = sb.parseJson(original)
  chrome(ops, dir)
  local schema = "/gui/devices/children/list/schema/"
  add(ops, schema .. "unselectedBG", dir .. "deviceback.png?multiply=222")
  add(ops, schema .. "listTemplate/background/file", dir .. "deviceback.png?multiply=222")
  local button = schema .. "listTemplate/button/"
  for k, v in pairs(sb.parseJson(layout).gui.devices.children.list.schema.listTemplate.button) do
    if type(v) == "string" and v:sub(1,#dest) == dest then add(ops, button .. k, dir .. v:sub(#dest + 1)) end
  end
  ops[#ops + 1] = {op="replace",path="/scripts/0",value=private .. "voice.lua"}
  assets.add(private .. "voice.lua", script)
  attach("voice", dest .. "voicechat.config", sb.parseJson(layout), ops)
  attach("voice-locale", dest .. "locale.json", sb.parseJson(locale), sb.parseJson(words))
end

local function category()
  local target = "/binds/asterbound.binds"
  local config = bytes(base, target, 1822, "f6dd158a0a8e04a6")
  if not config or not bytes(source, "/binds/opensb.binds.patch", 262, "b7160bfe3b168bcf") then return end
  -- The group's public ID stays opensb; use OpenUI's category styling without
  -- renaming this client or replacing the user's action bindings.
  attach("category", target, sb.parseJson(config), {{op="replace",path="/opensb/name",value="^0;^orange;Asterbound"}})
end

local function vanilla(path, size, hash)
  local owner = assets.origin(path)
  local meta = owner and assets.sourceMetadata(owner)
  return meta and meta.name == "base" and bytes(owner, path, size, hash)
end

local function replaceMenu(target, name, ops)
  local path = private .. name .. ".patch"
  assets.add(path, ops)
  assets.patchReplace(target, target .. ".patch", source, path)
end

local function optionsMenu()
  local target = options .. "optionsmenu.config"
  local original = bytes(source, target .. ".patch", 8141, "b76180c5e960c14f", true)
  if not original or not vanilla(target, 4854, "ca72b750ee033b9c")
      or not bytes(base, target .. ".patch", 2447, "9ccd60e0a68514a0", true) then return end
  local ops, retired, widened = jarray(), 0, 0
  for _, op in ipairs(sb.parseJson(original)) do
    if op.path:find("/paneLayout/showModBindings/", 1, true) == 1 then
      retired = retired + 1
    else
      if op.path == "/paneLayout/showKeybindings/base" or op.path == "/paneLayout/showKeybindings/hover" then
        op.value = op.value:gsub("bigbuttonshort", "bigbuttonlong")
        widened = widened + 1
      end
      ops[#ops + 1] = op
    end
  end
  -- One full-width Controls button now opens *both* game and mod bindings.
  if retired == 4 and widened == 2 then replaceMenu(target, "options", ops) end
end

local function graphicsMenu()
  local target = "/interface/windowconfig/graphicsmenu.config"
  local original = bytes(source, target .. ".patch", 8402, "0c342d67b125951e", true)
  if not original or not vanilla(target, 6730, "2d2955a2eed5bf53")
      or not bytes(base, target .. ".patch.lua", 4628, "0094aa38b920704e", true)
      or not images("/interface/graphicsmenu/", {{"body_improved_ui",1246,"95596d84f55a2fcf"},
          {"header",366,"4b63adb4676dde85"},{"footer",333,"92abb2e48d029919"}}) then return end
  -- The native output column has no equivalent in the old skin. Keep the
  -- author's left pixel panel, extend its flat interior, and move only its
  -- right border/gutter. The bundled header/footer are already 472px wide.
  local image = assets.image("/interface/graphicsmenu/body_improved_ui.png")
  local size = image:size()
  if size[1] ~= 236 or size[2] ~= 214 then return end
  local wide = image:process("?crop=20;0;21;214?scalenearest=472;1")
  wide:copyInto({0,0}, image:process("?crop=0;0;217;214"))
  wide:copyInto({453,0}, image:process("?crop=217;0;236;214"))
  local ops = sb.parseJson(original)
  local body, hdr = 0, 0
  for _, op in ipairs(ops) do
    if op.path == "/paneLayout/background/fileBody" then
      op.value = private .. "graphics-body.png"
      body = body + 1
    elseif op.path == "/paneLayout/hdrLabel/value" then
      op.value = "Float Scene Buffer"
      hdr = hdr + 1
    end
  end
  if body ~= 1 or hdr ~= 1 then return end
  for i, row in ipairs({"renderFps","renderOutput","renderExposure","hdrPaperWhite","hdrPeak"}) do
    local y = 207 - (i - 1) * 29
    add(ops, "/paneLayout/" .. row .. "Label/position", {354,y+9})
    add(ops, "/paneLayout/" .. row .. "Slider/position", {266,y-4})
    add(ops, "/paneLayout/" .. row .. "ValueLabel/position", {420,y-4})
  end
  add(ops, "/paneLayout/vsyncCheckbox/position", {274,67})
  add(ops, "/paneLayout/vsyncLabel/position", {287,67})
  add(ops, "/paneLayout/renderOutputStatusLabel/position", {354,52})
  assets.add(private .. "graphics-body.png", wide)
  replaceMenu(target, "graphics", ops)
end

optionsMenu()
graphicsMenu()
bindings()
shaders()
voice()
category()

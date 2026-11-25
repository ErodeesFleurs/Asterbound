-- These two authored panes put the vanilla-relative extra cosmetic slots over
-- real equipment or bag cells. Register after the base slot generator, using
-- each installed theme's own footer. No original widget ID is remapped.
local config = "/interface/windowconfig/playerinventory.config"
local trays = "/scripts/asterbound/compat/inventory_trays/"
local function known(path, name, version, size, expected, binary, source)
  source = source or assets.origin(path)
  local meta = source and assets.sourceMetadata(source)
  if not meta or meta.name ~= name or meta.version ~= version then return end
  local data = assets.sourceBytes(source, path)
  if not binary then data = data:gsub("\r\n", "\n") end
  if #data ~= size then return end
  local hash = 0xcbf29ce484222325
  for i = 1, #data do hash = (hash ~ data:byte(i)) * 0x100000001b3 end
  return hash == expected
end

if not known("/interface/windowconfig/inventory_cosmetics_patch.lua", "asterbound_base", "0.1.4.0",
    1713, 0x3471f075cd13e4d7) then return end

local function extend(prefix, id, name, version, width, height, size, hash, backingSize, backingHash)
  local footer = prefix .. "inventoryfooter.png"
  if not known(footer, name, version, size, hash, true)
      or not known(prefix .. "backingimagesocial.png", name, version, backingSize, backingHash, true) then return end
  local original = assets.image(footer)
  local dimensions = original:size()
  if dimensions[1] ~= width or dimensions[2] ~= height then return end
  local expanded = original:process(("?crop=0;10;%d;11?scalenearest=1;%d"):format(width, height + 24))
  expanded:copyInto({0, 0}, original:process(("?crop=0;0;%d;2"):format(width)))
  expanded:copyInto({0, 24}, original)
  assets.add(trays .. id .. ".png", expanded)
  return true
end

local attached
local redone = "Inventory Interface Redone"
if known(config, redone, "1.2", 11759, 0x3dffafd1907d6aeb) then
  attached = extend("/interface/inventory/", "redone", redone, "1.2", 306, 23,
    336, 0xf009a2b88f67d579, 245, 0xeab62e3c1257d1ba)
end
local arcana = "ArcanaXBk3k"
local patch = config .. ".patch"
local provider
local haveBk3k
for _, source in ipairs(assets.sourcePaths()) do
  local meta = assets.sourceMetadata(source)
  if meta and meta.name == arcana and meta.version == "1.0.1" then provider = source end
  if meta and meta.name == "bk3k_inventory" and meta.version == "1.26" then haveBk3k = source end
end
if provider and haveBk3k and known(patch, arcana, "1.0.1", 18314, 0xa64ac356c0d979fb, false, provider)
    and known(patch, "bk3k_inventory", "1.26", 19385, 0x77a17760bd52c6f3, false, haveBk3k) then
  if extend("/interface/mwh_arcanaBk3kInventory/", "arcana", arcana, "1.0.1", 260, 20,
      437, 0x93ea6a67e73ed205, 134, 0x48b155eb4dde8f6f) then attached = true end
end
if attached then assert(assets.patch(config, trays .. "layout.patch.lua")) end

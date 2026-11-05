-- Neia Race 1.1.1 ships its complete SpearThrow ability but omits the builder
-- registry entry. Its seven harpoons therefore build an altAbility without
-- scripts/class and fail in vanilla getAltAbility when actually equipped.
-- Restore only the missing entry to the mod's own authored implementation.
-- The same registration is present in Aegonian's Elithian Races Mod 2.3.12:
-- /items/buildscripts/weaponabilities.config.patch, Workshop 850109963.
-- No external ability code or unrelated Elithian gameplay is copied here.
local directory = "/items/active/weapons/melee/abilities/spear/spearthrow/"
local function neiaSource(path)
  local source = assets.origin(path)
  local metadata = source and assets.sourceMetadata(source)
  if metadata and metadata.name == "Neia Race" and metadata.version == "1.1.1" then
    return source
  end
end
local function pinned(path, source, length, expected)
  if assets.origin(path) ~= source then return false end
  local bytes = assets.sourceBytes(source, path)
  if #bytes ~= length then return false end
  local fingerprint = 0xcbf29ce484222325
  for i = 1, #bytes do
    fingerprint = (fingerprint ~ bytes:byte(i)) * 0x100000001b3
  end
  return fingerprint == expected
end
function patch(data)
  if data.spearthrow ~= nil then return data end
  local ability = directory .. "spearthrow.weaponability"
  local source = neiaSource(ability)
  if not source then return data end
  -- Actual Workshop 3617897486 raw byte pins, respectively SHA-256:
  -- 5ce3d3bfd21bad2471721c642f92aeb96597faf9bbf5cad39ebd23956042bc15
  -- d28805b99b1d3e92e01b53ba4bf589737af004e19454c3aa80b53683dd50ad85
  if not pinned(ability, source, 1374, 0x3cb2f69e5a0539b8)
      or not pinned(directory .. "spearthrow.lua", source, 9534, 0x92834d8791925404) then
    return data
  end
  for tier = 0, 6 do
    local name = tier == 0 and "neiaimprovisedharpoon" or "neiatier" .. tier .. "harpoon"
    local path = "/items/active/weapons/melee/harpoons/" .. name .. "/" .. name .. ".activeitem"
    if neiaSource(path) ~= source then return data end
    local item = sb.parseJson(assets.sourceBytes(source, path))
    if item.itemName ~= name or item.altAbilityType ~= "spearthrow" then return data end
  end
  data.spearthrow = ability
  return data
end

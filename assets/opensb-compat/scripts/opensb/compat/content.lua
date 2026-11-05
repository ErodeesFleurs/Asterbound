-- Source-backed repairs for the exact Workshop versions listed below. Rewrite
-- raw provider data, leaving registered translation/other patches to run once.
local function provider(path, name, version)
  local source = assets.origin(path)
  if not source then return end
  local metadata = assets.sourceMetadata(source)
  if metadata and metadata.name == name and metadata.version == version then
    return source
  end
end

local function replace(path, name, version, before, after, expected)
  local source = provider(path, name, version)
  if not source then return end
  local bytes = assets.sourceBytes(source, path)
  local escaped = before:gsub("(%W)", "%%%1")
  local repaired, count = bytes:gsub(escaped, function() return after end)
  if count == expected then
    assets.add(path, repaired)
  end
end

-- Neia's four splash projectiles already ship the intended sprites, but name
-- the projectile image field "frames" instead of "image".
local aqua = "/projectiles/staves/racialstaves/aqua/"
for _, entry in ipairs({
  {"splashspawner1.projectile", "splashspawner1.png"},
  {"splashspawner2.projectile", "splashspawner1.png"},
  {"splashdrop1/splashdrop1.projectile", "splashdrop1.png"},
  {"splashdrop2/splashdrop2.projectile", "splashdrop2.png"}
}) do
  replace(aqua .. entry[1], "Neia Race", "1.1.1",
    '"frames" : "' .. entry[2] .. '"',
    '"image" : "' .. entry[2] .. '"', 1)
end


if provider("/items/active/weapons/melee/abilities/spear/spearthrow/spearthrow.weaponability",
    "Neia Race", "1.1.1") then
  assets.patch("/items/buildscripts/weaponabilities.config", "/scripts/opensb/compat/neia-abilities.patch.lua")
end

-- Both fire-element staffs copied an override under a nonexistent emitter.
-- Match dragonstaff1's firecharge override and inherit the real staff.animation
-- fire particles, rather than supplying an empty or invented particle array.
for _, tier in ipairs({2, 3}) do
  local path = "/items/active/weapons/staff/dragonstaff" .. tier .. ".activeitem"
  local source = provider(path, "Draconis Race(Humanoid Dragon Race)",
    "Starbound 1.0 - Stable (Mod Version 1.5)")
  if source then
    local data = sb.parseJson(assets.sourceBytes(source, path))
    if data.elementalType == "fire" and data.animation == "/items/active/weapons/staff/staff.animation" then
      replace(path, "Draconis Race(Humanoid Dragon Race)",
        "Starbound 1.0 - Stable (Mod Version 1.5)",
        '"shadowcharge"', '"firecharge"', 1)
    end
  end
end

-- The raptor egg already carries its own functional pet spawner script; the
-- object still names an obsolete shared location instead of that local file.
local spawner = "/objects/generic/raptoregg/shipPetSpawner.lua"
if provider(spawner, "Draconis Race(Humanoid Dragon Race)",
    "Starbound 1.0 - Stable (Mod Version 1.5)") then
  replace("/objects/generic/raptoregg/dragonpetspawner.object",
    "Draconis Race(Humanoid Dragon Race)",
    "Starbound 1.0 - Stable (Mod Version 1.5)",
    '"/objects/spawner/shipPetSpawner.lua"', '"' .. spawner .. '"', 1)
end

-- Prop Pack ships the paired-bed destruction callback beside the object.
local bunkbed = "/objects/themed/barracks/barracksbunkbed/"
local upperBed = bunkbed .. "barracksbunkbedtop.object"
local upperBedSource = provider(upperBed, "PropPack", "1.2.1")
if upperBedSource and assets.origin(bunkbed .. "barracksbunkbedtop.lua") == upperBedSource then
  local bytes = assets.sourceBytes(upperBedSource, upperBed)
  local repaired, scripts = bytes:gsub('"/scripts/bunkbed%.lua"',
    '"' .. bunkbed .. 'barracksbunkbedtop.lua"')
  -- The upper half must not drop independently; vanilla's real empty pool
  -- replaces the nonexistent "nothing", without changing the lower half drop.
  local pools
  repaired, pools = repaired:gsub('"breakDropPool"%s*:%s*"nothing"', '"breakDropPool" : "empty"')
  if scripts == 1 and pools == 1 then assets.add(upperBed, repaired) end
end

-- The food maker uses the same configurable powered-machine contract as
-- Titancorp's foundry. Its accidentally copied ScriptPane testing.lua is not an
-- object implementation. Reuse Arcana's real assembler, keeping the authored
-- recipe config (including its industrial-component recipes) unchanged.
local assembler = "/scripts/automation/arcana_assembler.lua"
local assemblerSource = provider(assembler, "Arcana", "1.4.2.10")
if assemblerSource then
  local implementation = assets.sourceBytes(assemblerSource, assembler):gsub("\r\n", "\n")
  -- Actual Workshop 2359135864; normalized SHA-256
  -- 84317c93ed548f273fc291faf4af680a6a97b3ea369ffa03460ad409aa208524.
  local fingerprint = 0xcbf29ce484222325
  for i = 1, #implementation do
    fingerprint = (fingerprint ~ implementation:byte(i)) * 0x100000001b3
  end
  local foodmaker = "/objects/workshop/at_ext_foodmaker/at_ext_foodmaker.object"
  local source = provider(foodmaker, "[Arcana]Automation Extension", "2.4.4")
  if source and #implementation == 2981 and fingerprint == 0x1fa9542967ee1c06 then
    local data = sb.parseJson(assets.sourceBytes(source, foodmaker))
    if data.configPath == "/objects/workshop/at_ext_foodmaker/config.config"
        and data.objectType == "container" and data.slotCount == 3
        and data.maxPower == 30 and data.powerUseAmount == 15
        and data.animation == "animation.animation" then
      replace(foodmaker, "[Arcana]Automation Extension", "2.4.4",
        '"scripts" : [ "testing.lua" ]', '"scripts" : [ "' .. assembler .. '" ]', 1)
    end
  end
end

-- Halloween's bomb contains a Windows separator that is an illegal JSON escape.
local halloweenVersion = "1.5.6 Fixin' Da spookies"
replace("/projectiles/pumpbomb/pumpbomb.projectile", "Halloween", halloweenVersion,
  "/projectiles/explosions\\smokeexplosion/smokeexplosion.config",
  "/projectiles/explosions/smokeexplosion/smokeexplosion.config", 1)
replace("/items/active/weapons/melee/scythe/basicscythe/halbasicscythe.activeitem",
  "Halloween", halloweenVersion, '"basicscythe.png"', '"halbasicscythe.png"', 2)

-- Use the object's own first animation frame and the hats' actual wearable art.
-- No replacement images or missing item definitions are manufactured.
replace("/objects/holiday/hallowfountain/haltownfountain.object",
  "Halloween", halloweenVersion, '"inventoryIcon" : "fountainicon.png"',
  '"inventoryIcon" : "haltownfountain.png:default.0"', 1)
for _, entry in ipairs({
  {"invisible/unseenvali.head", "invisible"},
  {"theinvisibleman/invisibleman.head", "theinvisibleman"}
}) do
  local path = "/items/armors/decorative/hats/" .. entry[1]
  local sprite = "/items/armors/decorative/hats/" .. entry[2] .. "/head.png"
  if provider(sprite, "Halloween", halloweenVersion) then
    replace(path, "Halloween", halloweenVersion,
      '"inventoryIcon" : "icons.png:head"', '"inventoryIcon" : "head.png:normal"', 1)
  end
end

-- The cauldron animates default.0..7; the docking bay has default.1..3.
-- Supply only their missing preview aliases, retaining the real animation grid.
for _, entry in ipairs({
  {"/objects/misc/brewology_wizardryCauldron/cauldronDeco.frames", "mwh_brewology", "1.0.0.1", "default.default", "default.0"},
  {"/objects/landableships/landableships_dockingbay/default.frames", "landableships", "1.1", "default", "default.1"}
}) do
  local source = provider(entry[1], entry[2], entry[3])
  if source then
    local data = sb.parseJson(assets.sourceBytes(source, entry[1]))
    local names = data.frameGrid and data.frameGrid.names
    if names and names[1] and names[1][1] == entry[5] and not data.aliases then
      data.aliases = {[entry[4]] = entry[5]}
      assets.add(entry[1], data)
    end
  end
end

-- Titancorp 2.4.4 ships two byte-identical definitions of the same projectile.
-- Only the redundant filename is removed: the canonical projectile type and
-- every attack/explosion action remain unchanged. Both files are identical and
-- the mod's callers reference the type, never the redundant filename.
local primary = "/projectiles/loitering_munition_projectile/loitering_munition_projectile.projectile"
local duplicate = "/projectiles/explosions/loitering_munition_projectile/loitering_munition_projectile.projectile"
local source = provider(primary, "[Arcana]Automation Extension", "2.4.4")
if source and assets.origin(duplicate) == source then
  local bytes = assets.sourceBytes(source, primary)
  if bytes == assets.sourceBytes(source, duplicate) then
    assets.erase(duplicate)
  end
end

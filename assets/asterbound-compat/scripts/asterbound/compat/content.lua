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
  assets.patch("/items/buildscripts/weaponabilities.config", "/scripts/asterbound/compat/neia-abilities.patch.lua")
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

-- Compare complete verified source bytes, allowing only CRLF/LF differences.
local function knownBytes(bytes, size, expected)
  local normalized = bytes:gsub("\r\n", "\n")
  if #normalized ~= size then return false end
  local fingerprint = 0xcbf29ce484222325
  for i = 1, #normalized do
    fingerprint = (fingerprint ~ normalized:byte(i)) * 0x100000001b3
  end
  return fingerprint == expected
end

-- Official Titancorp 2.4.4 and 2.4.5 have byte-identical food-maker object,
-- recipe config and animation contracts. The missing testing.lua is not an
-- object implementation; the bundled at_ext_foodmaker.lua is a ScriptPane.
-- Reuse the same real Arcana assembler as Titancorp's foundry, preserving all
-- authored recipes (including motors), power parameters and wire transfer.
local titancorp = "[Arcana]Automation Extension"
local assembler = "/scripts/automation/arcana_assembler.lua"
local assemblerSource = provider(assembler, "Arcana", "1.4.2.10")
if assemblerSource then
  -- Actual Workshop 2359135864; normalized SHA-256
  -- 84317c93ed548f273fc291faf4af680a6a97b3ea369ffa03460ad409aa208524.
  local implementation = assets.sourceBytes(assemblerSource, assembler)
  local foodmaker = "/objects/workshop/at_ext_foodmaker/at_ext_foodmaker.object"
  local configPath = "/objects/workshop/at_ext_foodmaker/config.config"
  local source = provider(foodmaker, titancorp, "2.4.4")
    or provider(foodmaker, titancorp, "2.4.5")
  if source and assets.origin(configPath) == source
      and knownBytes(implementation, 2981, 0x1fa9542967ee1c06) then
    local bytes = assets.sourceBytes(source, foodmaker)
    -- Normalized SHA-256: object 615df2e30ad3f22a4e03354be2defb5da1e1e09356bf168c46928623f7eda324,
    -- config 771662c5918823c2fb7cb8d28406f1bd4fe0bd2c24f0ba60b0464591feb448cd.
    if knownBytes(bytes, 2493, 0xd95df6e101320667)
        and knownBytes(assets.sourceBytes(source, configPath), 2300, 0x0a1cb4824ea2f3b2) then
      local repaired, count = bytes:gsub('"scripts" : %[ "testing%.lua" %]',
        '"scripts" : [ "' .. assembler .. '" ]')
      if count == 1 then assets.add(foodmaker, repaired) end
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

-- Official Titancorp 2.4.4 and 2.4.5 ship the same byte-identical pair.
-- Remove only the redundant filename, after checking the complete known source
-- and raw equality of both effective providers. Callers use the projectile type;
-- every attack/explosion action in the canonical definition stays unchanged.
local primary = "/projectiles/loitering_munition_projectile/loitering_munition_projectile.projectile"
local duplicate = "/projectiles/explosions/loitering_munition_projectile/loitering_munition_projectile.projectile"
local source = provider(primary, titancorp, "2.4.4")
  or provider(primary, titancorp, "2.4.5")
if source and assets.origin(duplicate) == source then
  local bytes = assets.sourceBytes(source, primary)
  -- Normalized SHA-256 651313c2738c270583d5ecf493dad57384d9e011e6d60cc9084bbf4bfa1d3529.
  if knownBytes(bytes, 16519, 0x6763e6e661be3bb2)
      and bytes == assets.sourceBytes(source, duplicate) then
    assets.erase(duplicate)
  end
end

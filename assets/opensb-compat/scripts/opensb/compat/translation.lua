-- Source-owned repairs for the published patches listed below. Do not read the
-- patched target during onLoad: other mods may inspect it in their postLoad.
-- Replacing the registered slot preserves both earlier and later overlays.
local sources = {}
for _, path in ipairs(assets.sourcePaths()) do
  sources[path] = assets.sourceMetadata(path)
end
local installed = 0

local function fingerprint(bytes)
  local hash = 2166136261
  for i = 1, #bytes do
    hash = ((hash ~ bytes:byte(i)) * 16777619) & 0xffffffff
  end
  return hash
end

local function repair(name, version, path, length, hash, change)
  for source, metadata in pairs(sources) do
    if metadata.name == name and metadata.version == version then
      local ok, bytes = pcall(assets.sourceBytes, source, path)
      if ok and #bytes == length and fingerprint(bytes) == hash and assets.exists(path:sub(1, -7)) then
        local patch = sb.parseJson((bytes:gsub("^\239\187\191", "")))
        local replacement, replacementTarget = change(patch, path:sub(1, -7))
        if replacement then
          local target = path:sub(1, -7)
          local replacementPath = "/scripts/opensb/compat/translation/" .. name:gsub("[^%w_]", "_") .. path
          if type(replacement) == "string" then replacementPath = replacementPath .. ".lua" end
          assets.add(replacementPath, replacement)
          if assets.patchReplace(target, path, source, replacementPath, replacementTarget) then
            installed = installed + 1
          end
        end
      end
    end
  end
end

-- Official SChinese V108_241106_09 / UTC-241105-1953. The regex is an optional
-- textbox property, not a required pre-existing field. Keep all 18 labels and
-- the intended Unicode-capable 32-character constraint when a UI omits regex.
-- https://github.com/sffxzzp/Starbound-Chinese/releases/tag/UTC-241105-1953
repair("SChinese", "V108_241106_09", "/interface/windowconfig/charcreation.config.patch", 1714, 3687157494, function(patch)
  patch[17].op = "add"
  return patch
end)


local function provider(path, name, version)
  local source = assets.origin(path)
  local metadata = source and assets.sourceMetadata(source)
  return metadata and metadata.name == name and metadata.version == version
end

-- Starforge V.1.2 builds tooltipFields afresh and then merges
-- tooltipFieldsOverride. Translate the latter instead of the discarded old
-- tooltipFields; all descriptions, names, abilities and other overrides remain.
-- Workshop 2431875552 + 2957520130, items/buildscripts/starforge-builduniqueweapon.lua:151.
local function starforgeTooltip(patch, target)
  if not provider(target, "The Starforge v1.0", "V.1.2") then return end
  for _, operation in ipairs(patch) do
    if operation.path == "/tooltipFields/subtitle" then
      operation.path = "/tooltipFieldsOverride/subtitle"
    end
  end
  return patch
end

for _, entry in ipairs({
  {"/items/active/weapons/ranged/unique/behemoth/starforge-behemoth.activeitem.patch", 365, 3994709025},
  {"/items/active/weapons/ranged/legendary/frenzy/starforge-frenzy.activeitem.patch", 319, 2750621681},
  {"/items/active/weapons/ranged/unique/tidewalker/starforge-tidewalker.activeitem.patch", 289, 3449553169},
  {"/items/active/weapons/ranged/unique/lightofsorrow/starforge-lightofsorrow.activeitem.patch", 334, 1576409617},
  {"/items/active/weapons/ranged/unbound/unboundburstshotgun/unboundburstshotgun.activeitem.patch", 422, 419022840},
  {"/items/active/weapons/ranged/unbound/raventoxin/starforge-raventoxin.activeitem.patch", 352, 3332459597},
  {"/items/active/weapons/ranged/unique/bloodveil/bloodveil.activeitem.patch", 334, 839366318},
  {"/items/active/weapons/ranged/legendary/fever/starforge-fever.activeitem.patch", 319, 2496212815},
  {"/items/active/weapons/melee/broadsword/thalasso/gravediggershovel/gravediggershovel.activeitem.patch", 325, 2566325906},
  {"/items/active/weapons/ranged/legendary/ferment/starforge-ferment.activeitem.patch", 310, 136676372},
  {"/items/active/weapons/ranged/unique/tearoftheguidinglight/starforge-tearoftheguidinglight.activeitem.patch", 497, 1722336138}
}) do
  repair("StarforgeSchinese", "1.0", entry[1], entry[2], entry[3], starforgeTooltip)
end

-- These object descriptions are optional inspection overrides. Add the actual
-- published translations even when the English object relies on /description.
repair("StarforgeSchinese", "1.0", "/objects/dungeons/katahelruins/katahelthrone/katahelthrone.object.patch", 406, 1274783404, function(patch, target)
  if not provider(target, "The Starforge v1.0", "V.1.2") then return end
  patch[3].op = "add"
  return patch
end)

repair("StarforgeSchinese", "1.0", "/objects/yukai/other/yukaitidetuner/effigies/cephaloworm/cephalowormeffigy.object.patch", 1310, 3530218408, function(patch, target)
  if not provider(target, "The Starforge v1.0", "V.1.2") then return end
  for i = 3, 9 do patch[i].op = "add" end
  return patch
end)

-- The forgemaster hut is no longer advertised by Starforge's cockpit patch.
-- Guard only that obsolete entry, retaining the two translations after it.
repair("StarforgeSchinese", "1.0", "/interface/cockpit/cockpit.config.patch", 1154, 2043294281, function(patch)
  patch[4] = {
    {op = "test", path = "/displayDungeons/starforge-yukaiforgemasterhut"},
    patch[4]
  }
  return patch
end)

-- Spardan 1.2.4a supplies the ability definition through altAbilityType; the
-- item's empty altAbility object is the supported override location.
for _, entry in ipairs({
  {"/items/active/weapons/melee/hammer/sp-onihammer2/sp-onihammer2.activeitem.patch", 296, 130183989},
  {"/items/active/weapons/bow/sp-sagittarius/sp-sagittarius.activeitem.patch", 290, 1247424541}
}) do
  repair("Spardan ItemsCN", "1.0.0", entry[1], entry[2], entry[3], function(patch, target)
    if not provider(target, "Spardan", "1.2.4a") then return end
    patch[3].op = "add"
    return patch
  end)
end

-- Vanilla Fenerox has no effectDirectives field. Adding the published directive
-- enables the intended beam tint rather than creating any species content.
repair("TaehlTeleportBeams", "1.26", "/species/fenerox.species.patch", 108, 4114255077, function(patch)
  patch[1].op = "add"
  return patch
end)

-- RacEx88 uses vanilla's labelRandomise, not Xbawks' kawaRandomize.
repair("xbawks_Zh_CN", "2021.01.26", "/interface/windowconfig/charcreation.config.patch", 2226, 3593419016, function(patch, target)
  if not provider(target, "UI CharaCreate RacEx 88 arcana", "1.0.4") then return end
  patch[1].path = "/paneLayout/labelRandomise/value"
  return patch
end)

-- Voided 0.7.2 adds this action by identity, not by a stable array index. '-'
-- can append an action but cannot address the last action's label.
repair("voidedCN", "1.0.8", "/interface/windowconfig/keybindingsmenu.config.patch", 102, 520175514, function(patch)
  return {{
    op = "merge",
    path = "/keyActions/player",
    search = {action = "PlayerTechAction2"},
    value = {label = patch[1].value}
  }}
end)

-- bk3k inventory 1.26 has no inventory heading. Deepwell's final operation only
-- hides that heading off-screen; do not invent a widget or discard its other
-- 47 grid, portrait, equipment, stats and companion style operations.
repair("deepwellhud", "1.2", "/interface/windowconfig/playerinventory.config.patch", 4910, 2351658595, function(patch)
  local last = #patch
  patch[last] = {{op = "test", path = "/paneLayout/labelinventory"}, patch[last]}
  return patch
end)

-- Garden's object placements move when another biome mod inserts/removes a
-- placement. Tag the actual object sets, never a fixed array index (which can
-- now identify a tree). The biome reader consumes object parameters.
repair("GalacticAlmanach", "1.1", "/biomes/surface/garden.biome.patch", 1812, 4245741007, function()
  return assets.bytes("/scripts/opensb/compat/translation_garden.lua")
end)

-- Project45 v1.8.7b moved all six spawn/drop pool families out of the gacha
-- asset. Relocate the entire published Disable Loot patch, including dev kit
-- weights, to the real asset. Do not disable it or manufacture missing pools.
repair("project45disableloot", "0.9", "/treasure/project45/project45gacha.treasurepools.patch", 3810, 3327710443, function(patch)
  local target = "/treasure/project45/project45spawns.treasurepools"
  if not provider(target, "aikiro42-project45", "v1.8.7b") then return end
  return patch, target
end)

-- Mkio 1.71 intentionally omits kitsune inspection overrides, and the lamp
-- omits all racial inspection overrides. Add the published text at these
-- supported object fields; retain titles, descriptions and every other op.
for _, entry in ipairs({
  {"/objects/crafting/mkioworkshopcrafting/mkioworkshopcraftingmini.object.patch", 947, 4212401464},
  {"/objects/generic/bluoplasmalamp/bluoplasmalampA.object.patch", 1035, 1939111734},
  {"/objects/crafting/mk_furnituredyestation/mk_furnituredyestation.object.patch", 1095, 3105924534},
  {"/objects/crafting/mkioworkshopcrafting/mkioworkshopcrafting.object.patch", 938, 1554793985}
}) do
  repair("mkiotntworkshop_cn", "2022.5.29-1.05", entry[1], entry[2], entry[3], function(patch, target)
    if not provider(target, "mkiotntworkshop", "1.71") then return end
    for _, operation in ipairs(patch) do
      if operation.path:match("^/%a+Description$") then operation.op = "add" end
    end
    return patch
  end)
end

-- TerraLib 1.12 moved the biome registry from the stagehand configuration to
-- terra_biomes.json (terra_biomespread.lua:init reads it directly). Move all
-- three real Terraria biome definitions intact to that registry.
repair("terrariamod", "1.7", "/stagehands/terra_biomespread.stagehand.patch", 12228, 4096373927, function(patch)
  local target = "/terra_biomes.json"
  if not provider(target, "terralib", "1.12") then return end
  for _, operation in ipairs(patch) do
    operation.path = operation.path:sub(#"/biomes" + 1)
  end
  return patch, target
end)

local function knownJson(path, name, version, length, hash)
  if not provider(path, name, version) then return end
  local bytes = assets.sourceBytes(assets.origin(path), path)
  if #bytes ~= length or fingerprint(bytes) ~= hash then return end
  return sb.parseJson((bytes:gsub("^\239\187\191", "")))
end

-- The old 107-line dialog survives in epshadowtenantv, already translated by
-- this package's separate, identical legacy patch. The current tenant selects
-- a 77-line subset. Match original English identities, not obsolete numeric
-- positions; the removed lines remain translated in their actual legacy NPC.
repair("EP's Shadow Expansion_SChinese_Patch", "beta.1.0.1", "/npcs/tenants/epshadowtenant.npctype.patch", 5284, 1766244405, function(patch, target)
  local name = "EP's Expansion of Shadows"
  local version = "0.9.1.9 Consistancy Update Fixes 2"
  if not knownJson(target, name, version, 4043, 2038598330) then return end
  local legacyTarget = "/npcs/tenants/epshadowtenantv.npctype"
  local legacy = knownJson(legacyTarget, name, version, 5198, 745584430)
  if not legacy then return end
  local lines = legacy.scriptConfig.dialog.converse.default.default
  local translations = {}
  for _, operation in ipairs(patch) do
    local index = tonumber(operation.path:match("/(%d+)$"))
    translations[lines[index + 1]] = operation.value
  end
  -- This retained line received an apostrophe in the current subset.
  translations["It's calling..."] = translations["Its calling..."]
  local script = "local translatedByOriginal = sb.parseJson("
      .. string.format("%q", sb.printJson(translations)) .. ")\n"
      .. assets.bytes("/scripts/opensb/compat/translation_shadow_tenant.lua")
  return script
end)

-- Diverse Weather NPW 1.0 removes projectile slots 1..4 in both monsoons.
-- OptimizedWeather 0.0.1 also supplies whole weather files, so the actual
-- provider need not be Diverse Weather. Recognize only its published bytes.
-- Retain all particle optimizations and the surviving projectile rate, and
-- condition just the four obsolete rate edits on their projectile existing.
for _, entry in ipairs({
  {"/weather/isn_toxic_monsoon.weather.patch", 3043, 2510925974},
  {"/weather/isn_monsoon.weather.patch", 2962, 3471921223}
}) do
  local path = entry[1]
  repair("Optimized Weather - Diverse Weather", "Lukiwarble", path, 980, 172114326, function(patch, target)
    if not provider(target, "Diverse Weather", "1.3")
        and not knownJson(target, "OptimizedWeather", "0.0.1", entry[2], entry[3]) then return end
    local reduced = false
    for source, metadata in pairs(sources) do
      if metadata.name == "Diverse Weather NPW" and metadata.version == "1.0" then
        local bytes = assets.sourceBytes(source, path)
        reduced = #bytes == 191 and fingerprint(bytes) == 903916092
      end
    end
    if not reduced then return end
    for i = 8, 11 do
      local operation = patch[i]
      patch[i] = {{op = "test", path = operation.path}, operation}
    end
    return patch
  end)
end

-- Shadow 0.9.1.9 rewrote these books (four and three pages respectively).
-- The old translations' fourth/fifth pages no longer describe the live books.
-- Translate the current published English text instead of appending obsolete
-- lore merely to make the old numeric patch succeed. Only the book's own text
-- fields change; item configuration, icons and any extra overlay pages remain.
repair("EP's Shadow Expansion_SChinese_Patch", "beta.1.0.1", "/codex/epshadow/techofshadows.codex.patch", 2083, 2237147851, function(patch, target)
  if not knownJson(target, "EP's Expansion of Shadows", "0.9.1.9 Consistancy Update Fixes 2", 1515, 4259546434) then return end
  local pages = {
    "默影拥有着实令人好奇的科技。我的扫描未能识别它们，却能找到与其他种族科技的相似之处！虽然设计和构造大多不同，但仍有一些共同特征。我依然不明白它们如何运作，又以什么作为能源，不过我做了一些笔记。",
    "我先看了他们的飞船。和他们设计的一切一样，飞船的模样令人不安，与其说是机器，不如说更像活物。然而，它们使用人工智能，甚至与熟悉的SAIL有着相似的功能和数据库。其中似乎也包含了一些SAIL代码。不过，引擎看来并不使用厄尔吉斯燃料。",
    "他们的陆地载具也有些像悬浮车。较小、较快的那种在他们的语言里叫作“奔影”（由于他们说话的方式，写下来的读音完全不同，不过我已经另写了一篇关于这件事的笔记）；另一种能升到高空的则叫作“救世主”。两者都使用零重力引擎，并以某种晶体作为燃料。",
    "这些载具虽然令人惊叹，甚至有些吓人，却都没有武器。“救世主”的控制器似乎还有某种反复出现的故障，连默影自己也无法修好。"
  }
  patch[1].value = "默影的科技"
  patch[2].value = "一位旅行者的部分笔记。"
  local result = {patch[1], patch[2]}
  for index, text in ipairs(pages) do
    result[#result + 1] = {op = "replace", path = "/contentPages/" .. (index - 1), value = text}
  end
  return result
end)

repair("EP's Shadow Expansion_SChinese_Patch", "beta.1.0.1", "/codex/epshadow/silencesermon2alt.codex.patch", 1883, 3930369116, function(patch, target)
  if not knownJson(target, "EP's Expansion of Shadows", "0.9.1.9 Consistancy Update Fixes 2", 1248, 3883253991) then return end
  local pages = {
    "我发现了他们现身的规律。他们会被情感意义重大的地方吸引。凡是曾发生巨大苦痛的地方，他们就会出现。他们也带来了自己的科技产物：那些奇怪的机器，以及为其供能的晶体。我设法……嗯，收集了一些样本。科璃正在把这些东西送往空间站。",
    "我留在他们的一座前哨附近观察。那是一大片平地上的大型工地，建造方式十分奇特：清晨，他们的飞船进入轨道，向下发射了几根金属柱，径直砸入地下。大地震动了数个小时。震动停止后，更多默影出现在地表，带着类似物质操纵器的工具。",
    "他们挖去了一层土地，接着开始为将来的建筑打地基。他们运来了足够建起一座小镇的货箱和材料，但这里其实是一座矿场，并不是定居点。我需要靠近调查，并尽可能避免与默影接触。"
  }
  patch[1].value = "创始人的日记3"
  patch[2].value = "列夫·托拉普所有。"
  local result = {patch[1], patch[2]}
  for index, text in ipairs(pages) do
    result[#result + 1] = {op = "replace", path = "/contentPages/" .. (index - 1), value = text}
  end
  return result
end)

-- Multi-turrets 2.1 moved quest descriptions into scriptConfig and converted
-- conditionConfig to an array. Preserve all seven published Chinese strings.
repair("New Weapon Type-Siplified_Chinese", "1.1", "/quests/turretguns/celestialanchor/tg_celestialanchor.questtemplate.patch", 807, 3511406613, function(patch, target)
  if not provider(target, "Turret_Guns", "2.1") then return end
  for i = 4, 6 do patch[i].path = "/scriptConfig" .. patch[i].path end
  patch[7].path = "/scriptConfig/conditionConfig/0/description"
  return patch
end)

-- Project Redemption 12.5's first marine loadout has no backpack. Wardrobe's
-- published add addresses its name as if the container already existed.
-- Initialize only that supported equipment container when missing, retaining
-- existing backpack parameters/alternatives and every other Wardrobe group.
repair("IWUniWardrobe", "0.2.1.4", "/npcs/marine.npctype.patch", 8759, 2259821656, function(patch, target)
  if not provider(target, "Project Redemption", "12.5") then return end
  local operation = patch[3][1]
  local back = "/items/override/0/1/0/back"
  patch[3] = {
    {
      {op = "test", path = back, inverse = true},
      {op = "add", path = back, value = {{name = operation.value}}}
    },
    {
      {op = "test", path = back .. "/0", inverse = true},
      {op = "add", path = back .. "/-", value = {name = operation.value}}
    },
    operation
  }
  return patch
end)

-- Halloween swaps vanilla's part names: lights -> chassis (fullbright), and
-- chassis -> chassislit (ordinary body). Put Project45's existing images on
-- their corresponding real parts, preserving both mods' complete animation.
repair("aikiro42-project45", "v1.8.7b", "/objects/crafting/upgradeablecraftingobjects/craftinganvil/craftinganvil.animation.patch", 2092, 712403486, function(patch, target)
  if not knownJson(target, "Halloween", "1.5.6 Fixin' Da spookies", 5584, 3382471348) then return end
  patch[2].path = "/animatedParts/parts/chassislit/partStates/stage/project45replicator"
  patch[3].path = "/animatedParts/parts/chassis/partStates/stage/project45replicator"
  local lightsOff = patch[8].value.properties.lightsOff
  lightsOff[#lightsOff + 1] = "halloweenglow"
  patch[#patch + 1] = {
    op = "add",
    path = "/animatedParts/stateTypes/stage/states/halloween/properties/lightsOff/-",
    value = "project45replicatorglow"
  }
  return patch
end)

-- Stardust Core Lite / Quickbar Mini repurposes mmupgradegui.config as the
-- quickbar and moves the real upgrade pane to mmupgradegui.original.config.
-- quickbar/icons.json:starbound:mattermanipulator opens that real pane.
-- Move all 21 SChinese operations intact; never add fake title widgets to the
-- quickbar or throw away the upgrade descriptions to suppress the exception.
repair("SChinese", "V108_241106_09", "/interface/scripted/mmupgrade/mmupgradegui.config.patch", 2130, 1423650660, function(patch)
  local target = "/interface/scripted/mmupgrade/mmupgradegui.original.config"
  if not knownJson(target, "QuickbarMini", "Beta v0.1.5h-aa94060", 13269, 2784593246) then return end
  return patch, target
end)

-- Black Armory 4.2.5 replaced the old eight-line default conversation with
-- racial conversations. Its human/black_felin lists retain the greeting,
-- combat, cat-employer and gun lines; preserve their published translations.
-- The outing now refers to a base/FOB, and the Felin employer line now says
-- "like me": translate those revised source lines faithfully. The removed
-- generic pay/AI/ultraviolet chatter is not restored as a fabricated fallback.
repair("The_Black_Armory_SChinese", "1.4。1", "/npcs/tenants/black_lightinfantry.npctype.patch", 1411, 2911619682, function(patch, target)
  if not knownJson(target, "The Black Armory", "4.2.5", 5606, 2839322470) then return end
  local human = {
    patch[1].value,
    "偶尔能离开前线基地走走也挺好的。",
    patch[3].value,
    patch[7].value,
    patch[8].value
  }
  local felin = {
    patch[1].value,
    "偶尔能离开基地走走也挺好的。",
    patch[3].value,
    "我的老板和我一样也是猫人，这感觉挺不错的。",
    patch[8].value
  }
  local result = {}
  for _, entry in ipairs({{"human", human}, {"black_felin", felin}}) do
    for index, text in ipairs(entry[2]) do
      result[#result + 1] = {
        op = "replace",
        path = "/scriptConfig/dialog/converse/" .. entry[1] .. "/default/" .. (index - 1),
        value = text
      }
    end
  end
  return result
end)

if installed > 0 then
  sb.logInfo("[OpenStarbound Compatibility] Repaired %s source-owned translation/UI patches", installed)
end
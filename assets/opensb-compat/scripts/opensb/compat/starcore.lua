-- StarCore V108_241106_09 imports util from replaceTags, while util imports
-- replaceTags. All four imports below are unused by sb_replaceTags; keep the
-- function body and util's real dependency unchanged.
-- Upstream: https://github.com/sffxzzp/Starbound-Chinese/commit/1f15ce27583c8cb2d9e43d3be91bbd74774b3fd8
local path = "/scripts/replaceTags.lua"
local source = assets.origin(path)
if not source then return end

local metadata = assets.sourceMetadata(source)
if not metadata or metadata.name ~= "SChinese" or metadata.version ~= "V108_241106_09" then
  return
end

-- Check the actual provider, not merely whether SChinese is installed: another
-- mod's replacement must not be rewritten. Unknown versions remain untouched.
local script = assets.bytes(path)
local imports = '^require%("/scripts/util%.lua"%)\r?\n'
    .. 'require%("/scripts/rect%.lua"%)\r?\n'
    .. 'require%("/scripts/quest/paramtext%.lua"%)\r?\n'
    .. 'require%("/scripts/quest/directions%.lua"%)\r?\n'
local bodyStart = script:match(imports .. "()")
if not bodyStart then
  sb.logWarn("[OpenStarbound Compatibility] SChinese replaceTags.lua has an unrecognized import header; left unchanged")
  return
end

assets.add(path, script:sub(bodyStart))
sb.logInfo("[OpenStarbound Compatibility] Removed unused SChinese replaceTags.lua imports to break the util require cycle")

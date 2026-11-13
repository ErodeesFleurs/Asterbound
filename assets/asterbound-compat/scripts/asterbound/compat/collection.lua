-- Repairs are data, not a general Lua migration pass. Every entry identifies
-- an exact provider/version and complete original source fingerprint. Unknown
-- content is left intact; all edits for one asset must match before publishing.
local function fingerprint(bytes)
  local hash = 0xcbf29ce484222325
  for i = 1, #bytes do
    hash = (hash ~ bytes:byte(i)) * 0x100000001b3
  end
  return string.format("%016x", hash)
end

local function matches(source, rule)
  local metadata = source and assets.sourceMetadata(source)
  return metadata and metadata.name == rule.name and metadata.version == rule.version
end

local function known(bytes, rule)
  local normalized = rule.binary and bytes or bytes:gsub("\r\n", "\n")
  if #normalized == rule.size and fingerprint(normalized) == rule.hash then
    return normalized
  end
end

local sources = {}
local unlocks = {}
for _, source in ipairs(assets.sourcePaths()) do
  local metadata = assets.sourceMetadata(source)
  if metadata then sources[source] = metadata end
end

local function sourceFor(rule)
  -- Patch slots and metadata scripts hold their original provider, regardless
  -- of later sources shadowing the same virtual filename.
  if rule.kind == "patch" or rule.kind == "loadscript" or rule.kind == "recipes" then
    for source, metadata in pairs(sources) do
      if metadata.name == rule.name and metadata.version == rule.version then return source end
    end
  else
    return assets.origin(rule.path)
  end
end

local function effectiveGuard(rule)
  local source = sourceFor(rule)
  if not matches(source, rule) then return false end
  if rule.hash then
    local ok, bytes = pcall(assets.sourceBytes, source, rule.path)
    return ok and known(bytes, rule) ~= nil
  end
  return true
end

local function replaceAll(bytes, edit)
  local chunks, start, count = {}, 1, 0
  if edit.before == "" then return end
  while true do
    local first, last = bytes:find(edit.before, start, true)
    if not first then break end
    count = count + 1
    chunks[#chunks + 1] = bytes:sub(start, first - 1)
    chunks[#chunks + 1] = edit.after
    start = last + 1
  end
  if count ~= edit.count then return end
  chunks[#chunks + 1] = bytes:sub(start)
  return table.concat(chunks)
end


local function repair(rule, id)
  local source = sourceFor(rule)
  if not matches(source, rule) then return end
  if rule.targetName then
    local targetSource = assets.origin(rule.target)
    local metadata = targetSource and assets.sourceMetadata(targetSource)
    if not metadata or metadata.name ~= rule.targetName or metadata.version ~= rule.targetVersion then return end
  end
  for _, guard in ipairs(rule.guards or {}) do
    if not effectiveGuard(guard) then return end
  end
  if rule.anyGuards then
    local matched = false
    for _, guard in ipairs(rule.anyGuards) do
      if effectiveGuard(guard) then matched = true; break end
    end
    if not matched then return end
  end
  local ok, original = pcall(assets.sourceBytes, source, rule.path)
  if not ok then return end
  local bytes = known(original, rule)
  if not bytes then
    sb.logWarn("[Asterbound Compatibility] Collection source changed; left intact: %s", rule.path)
    return
  end

  if rule.kind == "erase" then
    local canonical = assets.origin(rule.canonical)
    if not canonical then return end
    if canonical ~= source then
      local guarded = false
      for _, guard in ipairs(rule.guards or {}) do
        if guard.path == rule.canonical and guard.hash and guard.size and guard.kind ~= "patch" then
          guarded = true
          break
        end
      end
      if not guarded then return end
    end
    local other = assets.sourceBytes(canonical, rule.canonical):gsub("\r\n", "\n")
    if other ~= bytes then return end
    assets.erase(rule.path)
  else
    for _, edit in ipairs(rule.edits) do
      bytes = replaceAll(bytes, edit)
      if not bytes then
        sb.logWarn("[Asterbound Compatibility] Collection repair site changed; left intact: %s", rule.path)
        return
      end
    end
    if original:find("\r\n", 1, true) then bytes = bytes:gsub("\n", "\r\n") end
    local patch = rule.kind == "patch" or rule.kind == "recipes"
    if patch or rule.kind == "loadscript" then
      local suffix = patch and rule.path:sub(-4) ~= ".lua" and ".patch" or ".lua"
      local replacement = "/scripts/asterbound/compat/collection/" .. id .. suffix
      assets.add(replacement, bytes)
      if patch then
        if not assets.patchReplace(rule.target, rule.path, source, replacement, rule.replacementTarget) then return end
        -- Old inline recipes become real database entries with their authored
        -- inputs, output parameters and matching crafting filter groups.
        for i, recipe in ipairs(rule.recipes or {}) do
          -- Reuse guarded canonical recipes: native identity excludes groups.
          if not recipe.existing then
            assets.add("/recipes/asterbound-compat/collection/" .. id .. "-" .. i .. ".recipe", recipe)
          end
          if rule.unlockRecipes then unlocks[recipe.output.item] = true end
        end
      else
        if not assets.loadScriptReplace(source, rule.path, replacement) then return end
      end
    else
      assets.add(rule.path, bytes)
    end
  end
  sb.logInfo("[Asterbound Compatibility] Repaired collection asset %s (%s)", rule.path, rule.name)
end

for _, group in ipairs({"runtime", "arcana", "lua", "duplicates", "content", "recipes"}) do
  local rules = assets.json("/scripts/asterbound/compat/collection_" .. group .. ".config")
  for i, rule in ipairs(rules) do
    repair(rule, group .. "-" .. i)
  end
end

if next(unlocks) then
  local items = {}
  for item in pairs(unlocks) do items[#items + 1] = item end
  table.sort(items)
  assets.add("/scripts/asterbound/compat/collection_blueprints.config", items)
  assert(assets.patch("/player.config", "/scripts/asterbound/compat/collection_blueprints.patch.lua"))
end
